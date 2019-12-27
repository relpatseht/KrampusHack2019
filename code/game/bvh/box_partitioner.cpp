#include "box_partitioner_internal.h"
#include "box_partitioner_sse2.h"
#include "../apply_permutation.h"
#include "../Assert.h"
#include <intrin.h>
#include <malloc.h>
#include <numeric>
#include <algorithm>

namespace
{

static __forceinline size_t RoundUp64(size_t val)
{
	return (val + 63) & ~63;
}

template <typename T>
static T* AllocWorkMem(uint count, void** workMemPtr)
{
	uintptr_t workMemAddr = reinterpret_cast<uintptr_t>(*workMemPtr);
	workMemAddr = RoundUp64(workMemAddr);

	T* mem = reinterpret_cast<T*>(workMemAddr);
	workMemAddr += count * sizeof(T);
	*workMemPtr = reinterpret_cast<void*>(workMemAddr);

	return mem;
}

template <size_t D>
static void UpdateBoxPartitions( const uint meanCount, Boxes<D>* inoutBoxes, BoxMeans* inoutBoxMeans, uint* inoutIndices, uint* outPartitions, void* workMem )
{
	const uint lastMeanIndex = meanCount - 1;
	unsigned* const partBegin = AllocWorkMem<unsigned>( inoutBoxes->count, &workMem );
	unsigned* const partEnd = partBegin + inoutBoxes->count;

	std::iota( partBegin, partEnd, 0 );

	unsigned* partCur = partBegin;
	for( size_t meanIndex = 0; meanIndex < lastMeanIndex; ++meanIndex )
	{
		partCur = std::partition( partCur, partEnd, [meanIndex, inoutBoxMeans]( unsigned boxIndex )
		{
			return inoutBoxMeans->nearestMeans[boxIndex] == meanIndex;
		} );
		outPartitions[meanIndex] = static_cast<uint>( partCur - partBegin );
	}
	outPartitions[lastMeanIndex] = inoutBoxes->count;

	apply_permutation( partBegin, inoutBoxMeans->nearestMeans, inoutBoxes->count );
	apply_permutation( partBegin, inoutBoxMeans->meanDists, inoutBoxes->count );
	apply_permutation( partBegin, inoutIndices, inoutBoxes->count );
	apply_permutation( partBegin, inoutBoxes->masses, inoutBoxes->count );

	for (size_t d = 0; d < D; ++d)
	{
		apply_permutation( partBegin, inoutBoxes->mins[d], inoutBoxes->count );
		apply_permutation( partBegin, inoutBoxes->maxs[d], inoutBoxes->count );
		apply_permutation( partBegin, inoutBoxes->weightedCenters[d], inoutBoxes->count );
	}
}

template <size_t D>
static size_t ComputeWorkMemSize( uint boxCount )
{
	static BoxPartitionDriver_SSE2<D> sse2Driver;

	boxCount = sse2Driver.AlignBoxCount( boxCount );
	const size_t nearestMeansSize = RoundUp64( sizeof( uint ) * boxCount );
	const size_t meanDistsSize = RoundUp64( sizeof( float ) * boxCount );
	const size_t permuteSize = RoundUp64( sizeof( unsigned ) * boxCount );
	const size_t boxWeightedCentersSize = RoundUp64( sizeof( float ) * boxCount ) * D;
	const size_t boxMassesSize = RoundUp64( sizeof( float ) * boxCount );
	const size_t meansSize = RoundUp64(sizeof( float )*boxCount*D) + (64 * D);

	return 64 + nearestMeansSize + meanDistsSize + permuteSize + boxWeightedCentersSize + boxMassesSize + meansSize;
}

// Partitions boxes of dimension D into K partitions
template <size_t D>
static bool PartitionBoxes( uint k, float* const* boxMins, float* const* boxMaxs, uint boxCount, uint* inoutIndices, uint* outPartitions, void* workMem )
{
	static const uint MAX_ITERATIONS = 64;
	static BoxPartitionDriver_SSE2<D> sse2Driver;

	const uint memBoxCount = sse2Driver.AlignBoxCount( boxCount );
	const uint memMeanCount = sse2Driver.AlignBoxCount( k );
	Boxes<D> boxes{ boxCount };
	Means<D> means{ k };
	BoxMeans boxMeans;

	assert( boxCount >= k );

	boxMeans.nearestMeans = AllocWorkMem<uint>( memBoxCount, &workMem );
	boxMeans.meanDists = AllocWorkMem<float>( memBoxCount, &workMem );
	boxes.masses = AllocWorkMem<float>( memBoxCount, &workMem );

	for( size_t d = 0; d < D; ++d )
	{
		means.means[d] = AllocWorkMem<float>( memMeanCount, &workMem );

		boxes.mins[d] = boxMins[d];
		boxes.maxs[d] = boxMaxs[d];
		boxes.weightedCenters[d] = AllocWorkMem<float>( memBoxCount, &workMem );
	}

	sse2Driver.InitializeBoxes( &boxes );
	sse2Driver.InitializeMeans( boxes, &means, &boxMeans, inoutIndices ); // indices used as work mem here

	sse2Driver.MakeIndexList( inoutIndices, boxCount, 0 );
	
	uint iteration;
	for( iteration = 0; iteration < MAX_ITERATIONS; ++iteration )
	{
		UpdateBoxPartitions<D>( k, &boxes, &boxMeans, inoutIndices, outPartitions, workMem );

		sse2Driver.ComputeMeans( boxes, outPartitions, &means );

		const bool converged = sse2Driver.UpdateNearestMeans( boxes, means, &boxMeans );

		if( converged )
			break;
	}

	uint partitionCount = 0;
	uint partitionBegin = 0;
	for( uint partitionIndex = 0; partitionIndex < means.k; ++partitionIndex )
	{
		const uint partitionEnd = outPartitions[partitionIndex];

		if( partitionBegin != partitionEnd )
		{
			++partitionCount;
			partitionBegin = partitionEnd;
		}
	}

	return iteration != MAX_ITERATIONS;
}

template <size_t D>
static bool PartitionBoxes( uint k, float* const* boxMins, float* const* boxMaxs, uint boxCount, uint* inoutIndices, uint* outPartitions )
{
	void* const workMem = _malloca( ComputeWorkMemSize<D>( boxCount ) );
	const bool ret = PartitionBoxes<D>( k, boxMins, boxMaxs, boxCount, inoutIndices, outPartitions, workMem );

	_freea( workMem );
	return ret;
}
}

size_t ComputePartitionLinesWorkMemSize( unsigned boxCount )
{
	return ComputeWorkMemSize<1>( boxCount );
}

size_t ComputePartitionBoxes2DWorkMemSize( unsigned boxCount )
{
	return ComputeWorkMemSize<2>( boxCount );
}

size_t ComputePartitionBoxes3DWorkMemSize( unsigned boxCount )
{
	return ComputeWorkMemSize<3>( boxCount );
}

bool PartitionLines( unsigned k, float* const* boxMins, float* const* boxMaxs, unsigned boxCount, unsigned* inoutIndices, unsigned* outPartitions, void* workMem )
{
	return PartitionBoxes<1>( k, boxMins, boxMaxs, boxCount, inoutIndices, outPartitions, workMem );
}
bool PartitionBoxes2D( unsigned k, float* const* boxMins, float* const* boxMaxs, unsigned boxCount, unsigned* inoutIndices, unsigned* outPartitions, void* workMem )
{
	return PartitionBoxes<2>( k, boxMins, boxMaxs, boxCount, inoutIndices, outPartitions, workMem );
}

bool PartitionBoxes3D( unsigned k, float* const* boxMins, float* const* boxMaxs, unsigned boxCount, unsigned* inoutIndices, unsigned* outPartitions, void* workMem )
{
	return PartitionBoxes<3>( k, boxMins, boxMaxs, boxCount, inoutIndices, outPartitions, workMem );
}

bool PartitionLines( unsigned k, float* const* boxMins, float* const* boxMaxs, unsigned boxCount, unsigned* inoutIndices, unsigned* outPartitions )
{
	return PartitionBoxes<1>( k, boxMins, boxMaxs, boxCount, inoutIndices, outPartitions );
}

bool PartitionBoxes2D( unsigned k, float* const* boxMins, float* const* boxMaxs, unsigned boxCount, unsigned* inoutIndices, unsigned* outPartitions )
{
	return PartitionBoxes<2>( k, boxMins, boxMaxs, boxCount, inoutIndices, outPartitions );
}

bool PartitionBoxes3D( unsigned k, float* const* boxMins, float* const* boxMaxs, unsigned boxCount, unsigned* inoutIndices, unsigned* outPartitions )
{
	return PartitionBoxes<3>( k, boxMins, boxMaxs, boxCount, inoutIndices, outPartitions );
}

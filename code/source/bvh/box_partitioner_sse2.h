#pragma once

#include "box_partitioner_internal.h"
#include "../Assert.h"
#include <xmmintrin.h>
#include <emmintrin.h>
#include <algorithm>
#include <cstring>

template <size_t D>
struct BoxPartitionDriver_SSE2 : BoxPartitionDriver<D>
{
private:
	// Computes the distance from the mean to the nearest point on the box.
	// Result is negative if mean is inside the box.
	static inline __m128 BoxToSingleMeanDistance( const Boxes<D>& boxes, uint sseBoxIndex, const Means<D>& means, uint meanIndex )
	{
		// The equation max(min-mean, 0, mean-max) will return the distance for
		// each axis to the bounding box, however, it will be 0 for touching or
		// inside the box, since inside the box, both planar distance are negative.
		// To compensate, record the larger (negative) planar distance on each axis.
		// If all axes are within the box, use the largest (negative, ie: closest to 0)
		// planar distance as the distance from the edge of the box and negate it.
		const __m128 zero = _mm_setzero_ps();
		__m128 distSqr = _mm_setzero_ps();
		__m128 minSideDist = _mm_set1_ps( std::numeric_limits<float>::lowest() );
		__m128 insideBox = _mm_castsi128_ps( _mm_set1_epi32( 0xFFFFFFFF ) );

		for( size_t d = 0; d < D; ++d )
		{
			const __m128 mean = _mm_set1_ps( means.means[d][meanIndex] );
			const __m128 boxMin = _mm_loadu_ps( boxes.mins[d] + ( sseBoxIndex << 2 ) );
			const __m128 boxMax = _mm_loadu_ps( boxes.maxs[d] + ( sseBoxIndex << 2 ) );
			const __m128 minToMeanDist = _mm_sub_ps( boxMin, mean );
			const __m128 meanToMaxDist = _mm_sub_ps( mean, boxMax );
			const __m128 minDistToSide = _mm_max_ps( minToMeanDist, meanToMaxDist ); // We use max, because these values are negative
			const __m128 minDistToBox = _mm_max_ps( minDistToSide, zero );
			const __m128 minDistToBoxSqr = _mm_mul_ps( minDistToBox, minDistToBox );
			const __m128 insideOnAxis = _mm_cmpeq_ps( minDistToBox, zero );

			insideBox = _mm_and_ps( insideOnAxis, insideBox );
			minSideDist = _mm_max_ps( minSideDist, minDistToSide );
			distSqr = _mm_add_ps( distSqr, minDistToBoxSqr );
		}

		__m128 dist = _mm_sqrt_ps( distSqr );
		dist = _mm_andnot_ps( insideBox, dist );
		minSideDist = _mm_and_ps( insideBox, minSideDist );

		return _mm_or_ps( dist, minSideDist );
	}

	static inline float FltPartitionGather( const float* values, unsigned partitionBegin, unsigned partitionEnd, unsigned ssePartitionBegin, unsigned ssePartitionEnd )
	{
		float gather = 0.0f;

		for( uint boxIndex = partitionBegin; boxIndex < ssePartitionBegin; ++boxIndex )
			gather += values[boxIndex];

		for( uint boxIndex = ssePartitionEnd; boxIndex < partitionEnd; ++boxIndex )
			gather += values[boxIndex];

		return gather;
	}

	static __forceinline float HAdd( __m128 v )
	{
		__m128 shuf = _mm_shuffle_ps( v, v, _MM_SHUFFLE( 2, 3, 0, 1 ) ); // [ C D | B A ]
		__m128 sums = _mm_add_ps( v, shuf ); // sums = [ D+C C+D | B+A A+B ]
		shuf = _mm_movehl_ps( shuf, sums ); //         [   C   D | D+C C+D ]
		sums = _mm_add_ss( sums, shuf );

		return _mm_cvtss_f32( sums );
	}

	__forceinline uint SSEBoxCount( uint boxCount )
	{
		return AlignBoxCount( boxCount ) >> 2;
	}

public:
	virtual void MakeIndexList( unsigned* start, unsigned count, unsigned valStart ) final
	{
		if(count > 4)
		{
			const unsigned sseCount = SSEBoxCount( count ) - 1;
			__m128i* sseIndexCur = reinterpret_cast<__m128i*>( start ) + 1;
			alignas( 64 ) __m128i inc = _mm_set1_epi32( 4 );

			std::iota( start, start + 4, valStart );
			for( size_t sseIndex = 1; sseIndex < sseCount; ++sseIndex )
				*sseIndexCur++ = _mm_add_epi32( sseIndexCur[-1], inc );

			const unsigned completed = sseCount << 2;
			start += completed;
			count -= completed;
			valStart += completed;
		}
		
		std::iota( start, start + count, valStart );
	}

	virtual void InitializeBoxes( Boxes<D>* inoutBoxes ) final
	{
		const uint sseBoxCount = SSEBoxCount( inoutBoxes->count );
		__m128* const outMasses = reinterpret_cast<__m128*>( inoutBoxes->masses );
		const __m128 zero = _mm_setzero_ps();
		const __m128 half = _mm_set1_ps( 0.5f );

		for( uint sseBoxIndex = 0; sseBoxIndex < sseBoxCount; ++sseBoxIndex )
		{
			outMasses[sseBoxIndex] = zero;

			for( size_t x = 0; x < D - 1; ++x )
			{
				const __m128 xMin = _mm_loadu_ps( inoutBoxes->mins[x] + ( sseBoxIndex << 2 ) );
				const __m128 xMax = _mm_loadu_ps( inoutBoxes->maxs[x] + ( sseBoxIndex << 2 ) );
				const __m128 xExtent = _mm_sub_ps( xMax, xMin );

				for( size_t y = x + 1; y < D; ++y )
				{
					const __m128 yMin = _mm_loadu_ps( inoutBoxes->mins[y] + ( sseBoxIndex << 2 ) );
					const __m128 yMax = _mm_loadu_ps( inoutBoxes->maxs[y] + ( sseBoxIndex << 2 ) );
					const __m128 yExtent = _mm_sub_ps( yMax, yMin );
					const __m128 xyArea = _mm_mul_ps( xExtent, yExtent );

					outMasses[sseBoxIndex] = _mm_add_ps( outMasses[sseBoxIndex], xyArea );
				}
			}

			outMasses[sseBoxIndex] = _mm_max_ps( outMasses[sseBoxIndex], _mm_set1_ps( FLT_EPSILON ) );
		}

		for (size_t d = 0; d < D; ++d)
		{
			const float* const mins = inoutBoxes->mins[d];
			const float* const maxs = inoutBoxes->maxs[d];
			__m128* const outWeightedCenters = reinterpret_cast<__m128*>( inoutBoxes->weightedCenters[d] );

			for( uint sseBoxIndex = 0; sseBoxIndex < sseBoxCount; ++sseBoxIndex )
			{
				const __m128 min = _mm_loadu_ps( mins + ( sseBoxIndex << 2 ) );
				const __m128 max = _mm_loadu_ps( maxs + ( sseBoxIndex << 2 ) );
				const __m128 perim = _mm_add_ps( max, min );
				const __m128 halfMass = _mm_mul_ps( outMasses[sseBoxIndex], half );

				outWeightedCenters[sseBoxIndex] = _mm_mul_ps( perim, halfMass );
			}
		}
	}

	virtual void InitializeMeans( const Boxes<D>& boxes, Means<D>* outMeans, BoxMeans* outBoxMeans, uint* indices ) final
	{
		const uint meanCount = outMeans->k;
		const uint sseBoxCount = SSEBoxCount( boxes.count );
		__m128* const outSSEMeanDists = reinterpret_cast<__m128*>( outBoxMeans->meanDists );
		__m128i* const outSSENearestMeans = reinterpret_cast<__m128i*>( outBoxMeans->nearestMeans );
		auto InitializeMean = [outMeans, &boxes]( uint boxIndex, uint meanIndex )
		{
			for( size_t d = 0; d < D; ++d )
				outMeans->means[d][meanIndex] = boxes.weightedCenters[d][boxIndex] / boxes.masses[boxIndex];
		};

		InitializeMean( 0, 0 );

		MakeIndexList( indices, boxes.count - 1, 1 );

		std::memset( outBoxMeans->nearestMeans, 0, sizeof( uint ) * boxes.count );
		for( uint sseBoxIndex = 0; sseBoxIndex < sseBoxCount; ++sseBoxIndex )
			outSSEMeanDists[sseBoxIndex] = BoxToSingleMeanDistance( boxes, sseBoxIndex, *outMeans, 0 );

		for( uint meanIndex = 1; meanIndex < meanCount; ++meanIndex )
		{
			const __m128i sseMeanIndex = _mm_set1_epi32( static_cast<int>( meanIndex ) );
			uint* const farthestNearestIndexPtr = std::max_element( indices, indices + boxes.count - meanIndex, [outBoxMeans]( uint a, uint b )
			{
				return outBoxMeans->meanDists[a] < outBoxMeans->meanDists[b];
			} );
			const uint farthestNearestIndex = *farthestNearestIndexPtr;

			InitializeMean( farthestNearestIndex, meanIndex );
			*farthestNearestIndexPtr = indices[boxes.count - meanIndex - 1]; // Erase currently found index with the back

			for( uint sseBoxIndex = 0; sseBoxIndex < sseBoxCount; ++sseBoxIndex )
			{
				const __m128 distance = BoxToSingleMeanDistance( boxes, sseBoxIndex, *outMeans, meanIndex );
				const __m128 oldDistance = outSSEMeanDists[sseBoxIndex];
				const __m128i lessEqual = _mm_castps_si128( _mm_cmple_ps( distance, oldDistance ) );
				const __m128i oldNearest = outSSENearestMeans[sseBoxIndex];
				const __m128i updatedNearest = _mm_and_si128( lessEqual, sseMeanIndex );
				const __m128i retainedNearest = _mm_andnot_si128( lessEqual, oldNearest );

				// Less equal to encourage movement to new means and protect
				// against degernate cases (big boxes containg other boxes)
				outSSEMeanDists[sseBoxIndex] = _mm_min_ps( distance, oldDistance );
				outSSENearestMeans[sseBoxIndex] = _mm_or_si128( updatedNearest, retainedNearest );
			}
		}
	}

	virtual bool UpdateNearestMeans( const Boxes<D>& boxes, const Means<D>& means, BoxMeans* inoutBoxMeans ) final
	{
		const uint meanCount = means.k;
		const uint sseBoxCount = boxes.count >> 2;
		const uint handledBoxCount = sseBoxCount << 2;
		__m128* const inoutSSEMeanDists = reinterpret_cast<__m128*>( inoutBoxMeans->meanDists );
		__m128i* const inoutSSENearestMeans = reinterpret_cast<__m128i*>( inoutBoxMeans->nearestMeans );
		__m128i converged = _mm_set1_epi32( 0xFFFFFFFF );

		for( uint sseBoxIndex = 0; sseBoxIndex < sseBoxCount; ++sseBoxIndex )
		{
			__m128 curDistance = inoutSSEMeanDists[sseBoxIndex];
			__m128i curNearest = inoutSSENearestMeans[sseBoxIndex];

			for( uint meanIndex = 0; meanIndex < meanCount; ++meanIndex )
			{
				const __m128i sseMeanIndex = _mm_set1_epi32( static_cast<int>( meanIndex ) );
				const __m128 meanDistance = BoxToSingleMeanDistance( boxes, sseBoxIndex, means, meanIndex );
				const __m128i lessThanDist = _mm_castps_si128( _mm_cmplt_ps( meanDistance, curDistance ) );
				const __m128i updatedNearest = _mm_and_si128( lessThanDist, sseMeanIndex );
				const __m128i retainedNearest = _mm_andnot_si128( lessThanDist, curNearest );

				curDistance = _mm_min_ps( meanDistance, curDistance );
				curNearest = _mm_or_si128( updatedNearest, retainedNearest );
				converged = _mm_andnot_si128( lessThanDist, converged );
			}

			inoutSSEMeanDists[sseBoxIndex] = curDistance;
			inoutSSENearestMeans[sseBoxIndex] = curNearest;
		}

		if( handledBoxCount < boxes.count ) // tail
		{
			__m128 curDistance = inoutSSEMeanDists[sseBoxCount];
			__m128i curNearest = inoutSSENearestMeans[sseBoxCount];
			const __m128i tailMask = [&]()
			{
				switch( 4 - ( boxes.count - handledBoxCount ) )
				{
				case 1:
					return _mm_set_epi32( 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 );
				case 2:
					return _mm_set_epi32( 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 );
				case 3:
					return _mm_set_epi32( 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 );
				default:
					assert(0);
				}

				return _mm_setzero_si128();
			}
			();

			for( uint meanIndex = 0; meanIndex < meanCount; ++meanIndex )
			{
				const __m128i sseMeanIndex = _mm_set1_epi32( static_cast<int>( meanIndex ) );
				const __m128 meanDistance = BoxToSingleMeanDistance( boxes, sseBoxCount, means, meanIndex );
				const __m128i lessThanDist = _mm_andnot_si128( tailMask, _mm_castps_si128( _mm_cmplt_ps( meanDistance, curDistance ) ) );
				const __m128i updatedNearest = _mm_and_si128( lessThanDist, sseMeanIndex );
				const __m128i retainedNearest = _mm_andnot_si128( lessThanDist, curNearest );

				curDistance = _mm_min_ps( meanDistance, curDistance );
				curNearest = _mm_or_si128( updatedNearest, retainedNearest );
				converged = _mm_andnot_si128( lessThanDist, converged );
			}

			inoutSSEMeanDists[sseBoxCount] = curDistance;
			inoutSSENearestMeans[sseBoxCount] = curNearest;
		}

		alignas( 16 ) unsigned cv[4];
		_mm_store_si128( reinterpret_cast<__m128i*>( cv ), converged );

		return cv[0] == ~0u && cv[1] == ~0u && cv[2] == ~0u && cv[3] == ~0u;
	}

	virtual void ComputeMeans( const Boxes<D>& boxes, const uint* partitions, Means<D>* outMeans ) final
	{
		const uint meanCount = outMeans->k;
		const __m128* const sseBoxMasses = reinterpret_cast<const __m128*>( boxes.masses );

		uint partitionBegin = 0;

		for( uint meanIndex = 0; meanIndex < meanCount; ++meanIndex )
		{
			const uint partitionEnd = partitions[meanIndex];
			
			if( partitionEnd - partitionBegin <= 3 )
			{
				float partitionMass = 0.0f;

				for( uint boxIndex = partitionBegin; boxIndex < partitionEnd; ++boxIndex )
					partitionMass += boxes.masses[boxIndex];

				partitionMass = 1.0f / partitionMass;

				for( size_t d = 0; d < D; ++d )
				{
					const float* const weightedCenters = boxes.weightedCenters[d];
					float mean = 0.0f;
					for( uint boxIndex = partitionBegin; boxIndex < partitionEnd; ++boxIndex )
						mean += weightedCenters[boxIndex];

					outMeans->means[d][meanIndex] = mean * partitionMass;
				}
			}
			else
			{
				uint ssePartitionBegin = (partitionBegin + 3) & ~3;
				uint ssePartitionEnd = partitionEnd & ~3;
				uint sseBoxBegin = ssePartitionBegin >> 2;
				uint sseBoxEnd = ssePartitionEnd >> 2;
				float partitionMass = FltPartitionGather( boxes.masses, partitionBegin, partitionEnd, ssePartitionBegin, ssePartitionEnd );
				__m128 ssePartitionMass = _mm_set_ss( partitionMass );

				for ( uint sseBoxIndex = sseBoxBegin; sseBoxIndex < sseBoxEnd; ++sseBoxIndex )
					ssePartitionMass = _mm_add_ps( ssePartitionMass, sseBoxMasses[sseBoxIndex] );

				partitionMass = 1.0f / HAdd( ssePartitionMass );

				for ( size_t d = 0; d < D; ++d )
				{
					const float* const weightedCenters = boxes.weightedCenters[d];
					const __m128* const sseWeightedCenters = reinterpret_cast<const __m128*>(weightedCenters);
					float mean = FltPartitionGather( weightedCenters, partitionBegin, partitionEnd, ssePartitionBegin, ssePartitionEnd );
					__m128 sseMean = _mm_set_ss( mean );

					for ( uint sseBoxIndex = sseBoxBegin; sseBoxIndex < sseBoxEnd; ++sseBoxIndex )
						sseMean = _mm_add_ps( sseMean, sseWeightedCenters[sseBoxIndex] );

					outMeans->means[d][meanIndex] = HAdd( sseMean ) * partitionMass;
				}
			}

			partitionBegin = partitionEnd;
		}
	}

	virtual uint AlignBoxCount( uint boxCount ) final
	{
		return ( boxCount + 3 ) & ~3;
	}
};

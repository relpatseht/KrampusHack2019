#pragma once

#include <xmmintrin.h>
#include <emmintrin.h>
#include <algorithm>
#include <stack>
#include <limits>
#include "../Assert.h"

#ifdef max
#undef max
#endif //#ifdef max

#ifdef min
#undef min
#endif //#ifdef min

namespace spatial_tree_common
{
	template<typename R, typename ...A>
	struct func_traits_impl
	{
		static const size_t arity = sizeof...(A);
		typedef R return_type;

		template<size_t a>
		struct arg
		{
			typedef typename std::tuple_element<a, std::tuple<A...>>::type type;
		};
	};

	template<typename L>
	struct func_traits : func_traits<decltype(&L::operator())> {};

	template<typename R, typename C, typename ...A>
	struct func_traits<R(C::*)(A...)> : func_traits_impl<R, A...> {};

	template<typename R, typename C, typename ...A>
	struct func_traits<R(C::*)(A...) const> : func_traits_impl<R, A...> {};

	template<typename R, typename ...A>
	struct func_traits<R(*)(A...)> : func_traits_impl<R, A...> {};

template <typename U>
static U* WorkMemAlloc( size_t count, void** workMemPtr, size_t align = alignof( U ) )
{
	uintptr_t workAddr = reinterpret_cast<uintptr_t>( *workMemPtr );
	workAddr = ( workAddr + ( align - 1 ) ) & ~( align - 1 );
	U* const addr = reinterpret_cast<U*>( workAddr );

	workAddr += sizeof( U ) * count;
	*workMemPtr = reinterpret_cast<void*>( workAddr );

	return addr;
}

template <size_t D>
static void GatherMinMax( unsigned index, const float * ( &mins )[D], const float * ( &maxs )[D], float ( *outMins )[D], float ( *outMaxs )[D] )
{
	for( size_t d = 0; d < D; ++d )
	{
		( *outMins )[d] = mins[d][index];
		( *outMaxs )[d] = maxs[d][index];
	}
}

inline void AxialMinMax( const float* mins, const float* maxs, unsigned count, float* outMin, float* outMax )
{
	float min = std::numeric_limits<float>::max();
	float max = std::numeric_limits<float>::lowest();

	for( unsigned index = 0; index < count; ++index )
	{
		min = std::min( min, mins[index] );
		max = std::max( max, maxs[index] );
	}

	*outMin = min;
	*outMax = max;
}

template <size_t D>
static float SurfaceArea( const float* mins, const float* maxs )
{
	static const size_t MAX_X = D - 1;
	float sa = 0.0f;
	for( size_t x = 0; x < MAX_X; ++x )
	{
		const float xExtent = maxs[x] - mins[x];

		for( size_t y = x + 1; y < D; ++y )
		{
			const float yExtent = maxs[y] - mins[y];

			sa += xExtent * yExtent;
		}
	}

	return sa;
}

template <size_t D>
static float SurfaceArea( const float ( &mins )[D], const float ( &maxs )[D] )
{
	return SurfaceArea<D>( &mins[0], &maxs[0] );
}

template <size_t D, size_t MAX_NODES>
static void SurfaceAreas( unsigned count, const float * ( &mins )[D], const float * ( &maxs )[D], float ( *outSA )[MAX_NODES] )
{
	static const size_t MAX_X = D - 1;
	const unsigned sseCount = ( count + 3 ) >> 2;
	__m128* const outSSESA = reinterpret_cast<__m128*>( *outSA );

	std::memset( *outSA, 0, sizeof( float ) * count );

	for( unsigned sseBoxIndex = 0; sseBoxIndex < sseCount; ++sseBoxIndex )
	{
		for( size_t x = 0; x < MAX_X; ++x )
		{
			const __m128 sseXMin = _mm_loadu_ps( mins[x] + ( sseBoxIndex << 2 ) );
			const __m128 sseXMax = _mm_loadu_ps( maxs[x] + ( sseBoxIndex << 2 ) );
			const __m128 sseXExtent = _mm_sub_ps( sseXMax, sseXMin );

			for( size_t y = x + 1; y < D; ++y )
			{
				const __m128 sseYMin = _mm_loadu_ps( mins[y] + ( sseBoxIndex << 2 ) );
				const __m128 sseYMax = _mm_loadu_ps( maxs[y] + ( sseBoxIndex << 2 ) );
				const __m128 sseYExtent = _mm_sub_ps( sseYMax, sseYMin );
				const __m128 sseXYArea = _mm_mul_ps( sseXExtent, sseYExtent );

				outSSESA[sseBoxIndex] = _mm_add_ps( outSSESA[sseBoxIndex], sseXYArea );
			}
		}
	}
}

template <size_t D, size_t MAX_NODES>
static void SurfaceAreasHandleInvalid( unsigned count, const float ( &mins )[D][MAX_NODES], const float ( &maxs )[D][MAX_NODES], float ( *outSA )[MAX_NODES] )
{
	static const size_t MAX_X = D - 1;
	static const __m128 zero = _mm_setzero_ps();
	const unsigned sseCount = ( count + 3 ) >> 2;
	__m128* const outSSESA = reinterpret_cast<__m128*>( *outSA );

	std::memset( *outSA, 0, sizeof( float ) * count );

	for( unsigned sseBoxIndex = 0; sseBoxIndex < sseCount; ++sseBoxIndex )
	{
		for( size_t x = 0; x < MAX_X; ++x )
		{
			const __m128 sseXMin = _mm_loadu_ps( mins[x] + ( sseBoxIndex << 2 ) );
			const __m128 sseXMax = _mm_loadu_ps( maxs[x] + ( sseBoxIndex << 2 ) );
			const __m128 sseXExtent = _mm_sub_ps( sseXMax, sseXMin );
			const __m128 sseValidXExtent = _mm_max_ps( zero, sseXExtent );

			for( size_t y = x + 1; y < D; ++y )
			{
				const __m128 sseYMin = _mm_loadu_ps( mins[y] + ( sseBoxIndex << 2 ) );
				const __m128 sseYMax = _mm_loadu_ps( maxs[y] + ( sseBoxIndex << 2 ) );
				const __m128 sseYExtent = _mm_sub_ps( sseYMax, sseYMin );
				const __m128 sseValidYExtent = _mm_max_ps( zero, sseYExtent );
				const __m128 sseXYArea = _mm_mul_ps( sseValidXExtent, sseValidYExtent );

				outSSESA[sseBoxIndex] = _mm_add_ps( outSSESA[sseBoxIndex], sseXYArea );
			}
		}
	}
}

template <size_t D, size_t MAX_NODES>
static void IntersectionBoxes( const float* myMins, const float* myMaxs, unsigned count, const float * ( &mins )[D], const float * ( &maxs )[D], float ( *outMins )[D][MAX_NODES],
							   float ( *outMaxs )[D][MAX_NODES] )
{
	const unsigned sseCount = ( count + 3 ) >> 2;

	for( size_t d = 0; d < D; ++d )
	{
		const __m128 sseMyMin = _mm_set1_ps( myMins[d] );
		const __m128 sseMyMax = _mm_set1_ps( myMaxs[d] );
		const float* const rhsMins = reinterpret_cast<const float*>( mins[d] );
		const float* const rhsMaxs = reinterpret_cast<const float*>( maxs[d] );
		__m128* const sseOutMins = reinterpret_cast<__m128*>( ( *outMins )[d] );
		__m128* const sseOutMaxs = reinterpret_cast<__m128*>( ( *outMaxs )[d] );

		for( unsigned sseBoxIndex = 0; sseBoxIndex < sseCount; ++sseBoxIndex )
		{
			const __m128 sseRHSMin = _mm_loadu_ps( rhsMins + ( sseBoxIndex << 2 ) );
			const __m128 sseRHSMax = _mm_loadu_ps( rhsMaxs + ( sseBoxIndex << 2 ) );

			sseOutMins[sseBoxIndex] = _mm_max_ps( sseMyMin, sseRHSMin );
			sseOutMaxs[sseBoxIndex] = _mm_min_ps( sseMyMax, sseRHSMax );
		}
	}
}

template <size_t D, size_t MAX_NODES>
static void BoxDistancesSqr( const float* myMins, const float* myMaxs, unsigned count, const float * ( &mins )[D], const float * ( &maxs )[D], float ( *outDists )[MAX_NODES] )
{
	const unsigned sseCount = ( count + 3 ) >> 2;
	const __m128 half = _mm_set1_ps( 0.5f );
	const __m128 negative = _mm_set1_ps( 0x80000000 );
	__m128* outSSEDistSqr = reinterpret_cast<__m128*>( *outDists );

	std::memset( *outDists, 0, sizeof( float ) * count );

	for( size_t d = 0; d < D; ++d )
	{
		const float myHalfExtent = ( myMaxs[d] - myMins[d] ) * 0.5f;
		const float myCenter = ( myMaxs[d] + myMins[d] ) * 0.5f;
		const __m128 sseMyHalfExtent = _mm_set1_ps( myHalfExtent );
		const __m128 sseMyCenter = _mm_set1_ps( myCenter );
		const __m128* const sseMins = reinterpret_cast<const __m128*>( mins[d] );
		const __m128* const sseMaxs = reinterpret_cast<const __m128*>( maxs[d] );

		for( unsigned sseBoxIndex = 0; sseBoxIndex < sseCount; ++sseBoxIndex )
		{
			const __m128 sseExtent = _mm_sub_ps( sseMaxs[sseBoxIndex], sseMins[sseBoxIndex] );
			const __m128 ssePerim = _mm_add_ps( sseMaxs[sseBoxIndex], sseMins[sseBoxIndex] );
			const __m128 sseHalfExtent = _mm_mul_ps( sseExtent, half );
			const __m128 sseCenter = _mm_mul_ps( ssePerim, half );
			const __m128 sseCenterDiff = _mm_sub_ps( sseCenter, sseMyCenter );
			const __m128 sseCenterDiffAbs = _mm_andnot_ps( negative, sseCenterDiff );
			const __m128 sseExtentDim = _mm_add_ps( sseHalfExtent, sseMyHalfExtent );
			const __m128 sseDist = _mm_sub_ps( sseCenterDiffAbs, sseExtentDim );
			const __m128 sseValidDist = _mm_max_ps( _mm_setzero_ps(), sseDist );
			const __m128 sseDistSqr = _mm_mul_ps( sseValidDist, sseValidDist );

			outSSEDistSqr[sseBoxIndex] = _mm_add_ps( outSSEDistSqr[sseBoxIndex], sseDistSqr );
		}
	}
}

template <size_t D, size_t MAX_NODES>
static void PercentOverlap( float mySurfaceArea, const float* myMins, const float* myMaxs, unsigned count, const float* rhsSurfaceAreas, const float * ( &mins )[D], const float * ( &maxs )[D],
							float ( *outOverlap )[MAX_NODES] )
{
	alignas( 16 ) float intersectMins[D][MAX_NODES];
	alignas( 16 ) float intersectMaxs[D][MAX_NODES];
	alignas( 16 ) float intersectSurfaceAreas[MAX_NODES];
	const unsigned sseCount = ( count + 3 ) >> 2;
	const __m128 sseMySurfaceArea = _mm_set1_ps( mySurfaceArea );
	const __m128* const sseIntSAs = reinterpret_cast<const __m128*>( intersectSurfaceAreas );
	__m128* const sseOutOverlap = reinterpret_cast<__m128*>( *outOverlap );

	IntersectionBoxes( myMins, myMaxs, count, mins, maxs, &intersectMins, &intersectMaxs );
	SurfaceAreasHandleInvalid( count, intersectMins, intersectMaxs, &intersectSurfaceAreas );

	for( unsigned sseBoxIndex = 0; sseBoxIndex < sseCount; ++sseBoxIndex )
	{
		const __m128 sseRHSSA = _mm_loadu_ps( rhsSurfaceAreas + ( sseBoxIndex << 2 ) );
		const __m128 sseIntSA = sseIntSAs[sseBoxIndex];
		const __m128 sseTotalSA = _mm_add_ps( sseMySurfaceArea, sseRHSSA );
		const __m128 sseOverlapSA = _mm_sub_ps( sseTotalSA, sseIntSA );

		sseOutOverlap[sseBoxIndex] = _mm_div_ps( sseIntSA, sseOverlapSA );
	}
}

template <size_t D, size_t MAX_NODES>
static void PercentOverlap( const float* myMins, const float* myMaxs, unsigned count, const float* mins[D], const float* maxs[D], float ( *outOverlap )[MAX_NODES] )
{
	const float mySurfaceArea = SurfaceArea<D>( myMins, myMaxs );
	alignas( 16 ) float rhsSurfaceAreas[MAX_NODES];

	SurfaceAreas( count, mins, maxs, &rhsSurfaceAreas );
	PercentOverlap( mySurfaceArea, count, rhsSurfaceAreas, mins, maxs, outOverlap );
}

template <typename T, size_t D, size_t MAX_NODES>
struct TreeInterface
{
	template <typename Tree>
	static void GatherIntersections( const Tree& node, const float* queryMins, const float* queryMaxs, float epsilon, unsigned( *outIntersections )[MAX_NODES] )
	{
		const unsigned sseChildCount = ( node.child_count() + 3 ) >> 2;
		const __m128 sseEpsilon = _mm_set1_ps( epsilon );
		__m128* const sseOutInts = reinterpret_cast<__m128*>( outIntersections );

		std::memset( *outIntersections, 0xFFFFFFFF, sizeof( *outIntersections ) );

		for( size_t d = 0; d < D; ++d )
		{
			const __m128 sseQueryMins = _mm_sub_ps( _mm_set1_ps( queryMins[d] ), sseEpsilon );
			const __m128 sseQueryMaxs = _mm_add_ps( _mm_set1_ps( queryMaxs[d] ), sseEpsilon );
			const __m128* const sseChildMins = reinterpret_cast<const __m128*>( node.child_mins( d ) );
			const __m128* const sseChildMaxs = reinterpret_cast<const __m128*>( node.child_maxs( d ) );

			for( unsigned sseChildIndex = 0; sseChildIndex < sseChildCount; ++sseChildIndex )
			{
				const __m128 sseMaxGEMin = _mm_cmpge_ps( sseChildMaxs[sseChildIndex], sseQueryMins );
				const __m128 sseMinLEMax = _mm_cmple_ps( sseChildMins[sseChildIndex], sseQueryMaxs );
				const __m128 sseChildInt = _mm_and_ps( sseMaxGEMin, sseMinLEMax );

				sseOutInts[sseChildIndex] = _mm_and_ps( sseOutInts[sseChildIndex], sseChildInt );
			}
		}
	}

	// ClampEnd true = segment intersections
	template<bool ClampEnd, typename Tree>
	static void GatherRayIntersections( const Tree &node, const float *start, const float *invDir, float epsilon, unsigned ( *outIntersections )[MAX_NODES] )
	{
		static const size_t SSE_MAX_NODES = (MAX_NODES + 3) >> 2;
		const unsigned sseChildCount = ( node.child_count() + 3 ) >> 2;
		const __m128 sseEpsilon = _mm_set1_ps( epsilon );
		__m128 sseEndClamp;
		__m128 * const sseOutInts = reinterpret_cast<__m128*>( outIntersections );
		__m128 sseMins[SSE_MAX_NODES];
		__m128 sseMaxs[SSE_MAX_NODES];

		std::memset( *outIntersections, 0xFFFFFFFF, sizeof( *outIntersections ) );

		if constexpr ( ClampEnd )
			sseEndClamp = _mm_set1_ps( 1.0f );
		else 
			sseEndClamp = _mm_set1_ps( INFINITY );

		for ( unsigned sseChildIndex = 0; sseChildIndex < sseChildCount; ++sseChildIndex )
		{
			sseMins[sseChildIndex] = _mm_set1_ps( -FLT_MAX );
			sseMaxs[sseChildIndex] = _mm_set1_ps( FLT_MAX );
		}

		for ( size_t d = 0; d < D; ++d )
		{
			const __m128 sseStart = _mm_set1_ps( start[d] );
			const __m128 sseInvDir = _mm_set1_ps( invDir[d] );
			const __m128 * const sseChildMins = reinterpret_cast<const __m128*>(node.child_mins( d ));
			const __m128 * const sseChildMaxs = reinterpret_cast<const __m128*>(node.child_maxs( d ));

			for ( unsigned sseChildIndex = 0; sseChildIndex < sseChildCount; ++sseChildIndex )
			{
				const __m128 sseCurChildMins = _mm_sub_ps( sseChildMins[sseChildIndex], sseEpsilon );
				const __m128 sseCurChildMaxs = _mm_add_ps( sseChildMaxs[sseChildIndex], sseEpsilon );
				const __m128 sseTEnter = _mm_mul_ps( _mm_sub_ps( sseCurChildMins, sseStart ), sseInvDir );
				const __m128 sseTExit = _mm_mul_ps( _mm_sub_ps( sseCurChildMaxs, sseStart ), sseInvDir );
				const __m128 sseTMin = _mm_min_ps( sseTEnter, sseTExit );
				const __m128 sseTMax = _mm_max_ps( sseTEnter, sseTExit );

				sseMins[sseChildIndex] = _mm_max_ps( sseTMin, sseMins[sseChildIndex] );
				sseMaxs[sseChildIndex] = _mm_min_ps( sseTMax, sseMaxs[sseChildIndex] );
			}
		}

		for ( unsigned sseChildIndex = 0; sseChildIndex < sseChildCount; ++sseChildIndex )
		{
			sseMins[sseChildIndex] = _mm_max_ps( sseMins[sseChildIndex], _mm_setzero_ps() );
			sseMaxs[sseChildIndex] = _mm_min_ps( sseMaxs[sseChildIndex], sseEndClamp );
			sseOutInts[sseChildIndex] = _mm_cmpge_ps( sseMaxs[sseChildIndex], sseMins[sseChildIndex] );
		}
	}

	template <typename Tree, typename = std::enable_if_t<D==3>>
	static void GatherPlanarIntersections( const Tree& node, const float (&plane)[4], float epsilon, unsigned( *outIntersections )[MAX_NODES] )
	{
		static const unsigned sseMaxNodes = (MAX_NODES + 3) >> 2;
		const unsigned sseChildCount = ( node.child_count() + 3 ) >> 2;
		const __m128 eps = _mm_set1_ps( epsilon );
		const __m128 half = _mm_set1_ps( 0.5f );
		const __m128 posMask = _mm_castsi128_ps( _mm_set1_epi32( 0x7FFFFFFF ) );
		const __m128 negPlaneD = _mm_set1_ps( -plane[3] );
		__m128 sseChildBoxExtentProjLen[sseMaxNodes] = {};
		__m128 sseChildBoxProjCenter[sseMaxNodes] = {};
		__m128* const sseOutInts = reinterpret_cast<__m128*>( outIntersections );

		for( size_t d = 0; d < D; ++d )
		{
			const __m128* const sseChildMins = reinterpret_cast<const __m128*>( node.child_mins( d ) );
			const __m128* const sseChildMaxs = reinterpret_cast<const __m128*>( node.child_maxs( d ) );
			const __m128 planeAxis = _mm_set1_ps( plane[d] );

			for( unsigned sseChildIndex = 0; sseChildIndex < sseChildCount; ++sseChildIndex )
			{
				const __m128 sseChildBoxMax = sseChildMaxs[sseChildIndex];
				const __m128 sseChildBoxMin = sseChildMins[sseChildIndex];
				const __m128 sseChildBoxExtent = _mm_mul_ps( _mm_sub_ps( sseChildBoxMax, sseChildBoxMin ), half );
				const __m128 sseChildBoxCenter = _mm_add_ps( sseChildBoxMin, sseChildBoxExtent );
				const __m128 sseChildBoxAxisExtentProjLen = _mm_and_ps( _mm_mul_ps( planeAxis, sseChildBoxExtent ), posMask );
				const __m128 sseChildBoxAxisProjCenter = _mm_mul_ps( planeAxis, sseChildBoxCenter );

				sseChildBoxExtentProjLen[sseChildIndex] = _mm_add_ps( sseChildBoxExtentProjLen[sseChildIndex], sseChildBoxAxisExtentProjLen );
				sseChildBoxProjCenter[sseChildIndex] = _mm_add_ps( sseChildBoxProjCenter[sseChildIndex], sseChildBoxAxisProjCenter );
			}
		}

		for ( unsigned sseChildIndex = 0; sseChildIndex < sseChildCount; ++sseChildIndex )
		{
			const __m128 sseChildBoxCenterPlaneDist = _mm_add_ps( sseChildBoxProjCenter[sseChildIndex], negPlaneD );
			const __m128 sseAbsChildBoxCenterPlaneDist = _mm_and_ps( sseChildBoxCenterPlaneDist, posMask );
			const __m128 sseAbsCenterPlaneDistEps = _mm_sub_ps( sseAbsChildBoxCenterPlaneDist, eps );

			sseOutInts[sseChildIndex] = _mm_cmple_ps( sseAbsCenterPlaneDistEps, sseChildBoxExtentProjLen[sseChildIndex] );
		}
	}

	template <typename Tree>
	static void GatherChildMinMaxs( const Tree& node, unsigned childIndex, float ( *outMins )[D], float ( *outMaxs )[D] )
	{
		for( size_t d = 0; d < D; ++d )
		{
			( *outMins )[d] = node.child_mins( d )[childIndex];
			( *outMaxs )[d] = node.child_maxs( d )[childIndex];
		}
	}

	template <typename Tree>
	static void TreeMinMax( const Tree& tree, float ( *outMins )[D], float ( *outMaxs )[D] )
	{
		for( size_t d = 0; d < D; ++d )
			AxialMinMax( tree.child_mins( d ), tree.child_maxs( d ), tree.child_count(), &( *outMins )[d], &( *outMaxs )[d] );
	}

	template <typename Tree>
	static void TreeMinMax( const Tree& tree, float* outMins, float* outMaxs )
	{
		for( size_t d = 0; d < D; ++d )
			AxialMinMax( tree.child_mins( d ), tree.child_maxs( d ), tree.child_count(), &outMins[d], &outMaxs[d] );
	}

	template <typename LHSTree, typename RHSTree, typename ClipFunc>
	static void Clip( LHSTree* lhsTree, RHSTree& rhsTree, float epsilon, const float ( &lhsMins )[D], const float ( &lhsMaxs )[D], const float ( &rhsMins )[D], const float ( &rhsMaxs )[D],
						ClipFunc &&ClipCallback )
	{
		struct NodePair
		{
			LHSTree *lhs;
			RHSTree *rhs;
			float lhsMins[D];
			float lhsMaxs[D];
			float rhsMins[D];
			float rhsMaxs[D];
		};
		std::stack<NodePair> nodeStack;
		NodePair * const root = &nodeStack.emplace( NodePair{ lhsTree, &rhsTree } );

		std::memcpy( root->lhsMins, lhsMins, sizeof( lhsMins ) );
		std::memcpy( root->lhsMaxs, lhsMaxs, sizeof( lhsMaxs ) );
		std::memcpy( root->rhsMins, rhsMins, sizeof( rhsMins ) );
		std::memcpy( root->rhsMaxs, rhsMaxs, sizeof( rhsMaxs ) );
		
		while ( !nodeStack.empty() )
		{
			const NodePair &nodes = nodeStack.top();
			LHSTree * const lhs = nodes.lhs;
			RHSTree * const rhs = nodes.rhs;
			const unsigned lhsChildCount = lhs->child_count();
			const unsigned rhsChildCount = rhs->child_count();
			bool leaves;
			bool stepDownLHS, stepDownRHS;

			if ( lhs->empty() || rhs->empty() )
			{
				nodeStack.pop();
				continue;
			}

			if( lhs->leaf_branch() || rhs->leaf_branch() )
			{
				leaves = true;
				stepDownLHS = rhs->leaf_branch();
				stepDownRHS = lhs->leaf_branch();
			}
			else
			{
				static const float SA_STEP_RATIO = 0.75f;
				const float rhsSurfaceArea = SurfaceArea( nodes.rhsMins, nodes.rhsMaxs );
				const float lhsSurfaceArea = SurfaceArea( nodes.lhsMins, nodes.lhsMaxs );

				leaves = false;
				stepDownLHS = lhsSurfaceArea > (rhsSurfaceArea * SA_STEP_RATIO);
				stepDownRHS = rhsSurfaceArea > (lhsSurfaceArea * SA_STEP_RATIO);
			}

			if ( stepDownLHS && stepDownRHS )
			{
				const bool selfClip = reinterpret_cast<uintptr_t>(lhs) == reinterpret_cast<uintptr_t>(rhs);
				nodeStack.pop();

				for ( unsigned lhsChildIndex = 0; lhsChildIndex < lhsChildCount; ++lhsChildIndex )
				{
					NodePair childPair;
					alignas(16) unsigned intersections[MAX_NODES];

					GatherChildMinMaxs( *lhs, lhsChildIndex, &childPair.lhsMins, &childPair.lhsMaxs );
					GatherIntersections( *rhs, childPair.lhsMins, childPair.lhsMaxs, epsilon, &intersections );

					if ( leaves )
					{
						const uint rhsStartIndex = selfClip ? lhsChildIndex + 1 : 0;

						for ( unsigned rhsChildIndex = rhsStartIndex; rhsChildIndex < rhsChildCount; ++rhsChildIndex )
						{
							if ( intersections[rhsChildIndex] )
							{
								if constexpr ( func_traits<ClipFunc>::arity == 6 )
								{
									float childRHSMins[D];
									float childRHSMaxs[D];

									GatherChildMinMaxs( *rhs, rhsChildIndex, &childRHSMins, &childRHSMaxs );

									if constexpr ( std::is_same_v<func_traits<ClipFunc>::return_type, bool> )
									{
										if ( !ClipCallback( lhs->leaf( lhsChildIndex ), rhs->leaf( rhsChildIndex ), childPair.lhsMins, childPair.lhsMaxs, childRHSMins, childRHSMaxs ) )
											return;
									}
									else
									{
										ClipCallback( lhs->leaf( lhsChildIndex ), rhs->leaf( rhsChildIndex ), childPair.lhsMins, childPair.lhsMaxs, childRHSMins, childRHSMaxs );
									}
								}
								else
								{
									if constexpr ( std::is_same_v<func_traits<ClipFunc>::return_type, bool> )
									{
										if ( !ClipCallback( lhs->leaf( lhsChildIndex ), rhs->leaf( rhsChildIndex ) ) )
											return;
									}
									else
									{
										ClipCallback( lhs->leaf( lhsChildIndex ), rhs->leaf( rhsChildIndex ) );
									}
								}
							}
						}
					}
					else
					{
						const uint rhsStartIndex = selfClip ? lhsChildIndex : 0;

						childPair.lhs = &lhs->subtree( lhsChildIndex );
						for ( unsigned rhsChildIndex = rhsStartIndex; rhsChildIndex < rhsChildCount; ++rhsChildIndex )
						{
							if ( intersections[rhsChildIndex] )
							{
								childPair.rhs = &rhs->subtree( rhsChildIndex );
								GatherChildMinMaxs( *rhs, rhsChildIndex, &childPair.rhsMins, &childPair.rhsMaxs );

								nodeStack.push( childPair );
							}
						}
					}
				}
			}
			else if( stepDownLHS )
			{
				NodePair childPair;
				alignas( 16 ) unsigned intersections[MAX_NODES];

				GatherIntersections( *lhs, nodes.rhsMins, nodes.rhsMaxs, epsilon, &intersections );

				childPair.rhs = rhs;
				std::memcpy( childPair.rhsMins, nodes.rhsMins, sizeof( childPair.rhsMins ) );
				std::memcpy( childPair.rhsMaxs, nodes.rhsMaxs, sizeof( childPair.rhsMaxs ) );
				nodeStack.pop();

				for( unsigned lhsChildIndex = 0; lhsChildIndex < lhsChildCount; ++lhsChildIndex )
				{
					if( intersections[lhsChildIndex] )
					{
						childPair.lhs = &lhs->subtree( lhsChildIndex );
						GatherChildMinMaxs( *lhs, lhsChildIndex, &childPair.lhsMins, &childPair.lhsMaxs );
						nodeStack.push( childPair );
					}
				}
			}
			else if ( stepDownRHS )
			{
				NodePair childPair;
				alignas(16) unsigned intersections[MAX_NODES];

				GatherIntersections( *rhs, lhsMins, lhsMaxs, epsilon, &intersections );

				childPair.lhs = lhs;
				std::memcpy( childPair.lhsMins, nodes.lhsMins, sizeof( childPair.lhsMins ) );
				std::memcpy( childPair.lhsMaxs, nodes.lhsMaxs, sizeof( childPair.lhsMaxs ) );
				nodeStack.pop();

				for ( unsigned rhsChildIndex = 0; rhsChildIndex < rhsChildCount; ++rhsChildIndex )
				{
					if ( intersections[rhsChildIndex] )
					{
						childPair.rhs = &rhs->subtree( rhsChildIndex );
						GatherChildMinMaxs( *rhs, rhsChildIndex, &childPair.rhsMins, &childPair.rhsMaxs );
						nodeStack.push( childPair );
					}
				}
			}
			else
			{
				nodeStack.pop(); // Can happen with NaN bounds. Just skip?
			}
		}
	}

	template <typename LHSTree, typename RHSTree, typename ClipFunc>
	static void Clip( const LHSTree& lhs, RHSTree& rhs, float epsilon, const float ( &lhsMins )[D], const float ( &lhsMaxs )[D], const float ( &rhsMins )[D], const float ( &rhsMaxs )[D],
						ClipFunc ClipCallback )
	{
		if constexpr ( func_traits<ClipFunc>::arity == 6 )
		{
			Clip( const_cast<LHSTree*>(&lhs), rhs, epsilon, lhsMins, lhsMaxs, rhsMins, rhsMaxs, [&ClipCallback] ( T& leaf, typename RHSTree::value_type& rhsLeaf, const float ( &lhsMins )[D], const float ( &lhsMaxs )[D], const float ( &rhsMins )[D], const float ( &rhsMaxs )[D] )
			{
				ClipCallback( const_cast<const T&>(leaf), rhsLeaf, lhsMins, lhsMaxs, rhsMins, rhsMaxs );
			} );
		}
		else
		{
			Clip( const_cast<LHSTree*>(&lhs), rhs, epsilon, lhsMins, lhsMaxs, rhsMins, rhsMaxs, [&ClipCallback] ( T& leaf, typename RHSTree::value_type& rhsLeaf )
			{
				ClipCallback( const_cast<const T&>(leaf), rhsLeaf );
			} );
		}
	}
	
	template <typename Tree>
	static size_t Size( const Tree& root )
	{
		std::stack<const Tree*> nodeStack;
		size_t size = 0;

		nodeStack.push( &root );

		while( !nodeStack.empty() )
		{
			const Tree& node = *nodeStack.top();

			nodeStack.pop();

			if( node.leaf_branch() )
				size += node.child_count();
			else
				for( unsigned childIndex = 0; childIndex < node.child_count(); ++childIndex )
					nodeStack.push( &node.subtree( childIndex ) );
		}

		return size;
	}

	template <typename Tree, typename NodeOp>
	static void NodeOp( Tree* root, NodeOp NodeCallback )
	{
		std::stack<Tree*> nodeStack;

		nodeStack.push( root );

		while( !nodeStack.empty() )
		{
			Tree* const node = nodeStack.top();

			nodeStack.pop();

			NodeCallback( *node );

			if( !node->leaf_branch() )
				for( unsigned childIndex = 0; childIndex < node->child_count(); ++childIndex )
					nodeStack.push( &node->subtree( childIndex ) );
		}
	}

	template <typename Tree, typename VisitFunc>
	static void IterateLeaves( Tree* root, VisitFunc VisitCallback )
	{
		std::stack<Tree*> nodeStack;

		nodeStack.push( root );

		while( !nodeStack.empty() )
		{
			Tree* const node = nodeStack.top();
			const unsigned childCount = node->child_count();

			nodeStack.pop();

			if( node->leaf_branch() )
			{
				for ( unsigned childIndex = 0; childIndex < childCount; ++childIndex )
				{
					if constexpr ( func_traits<VisitFunc>::arity == 3 )
					{
						float mins[D];
						float maxs[D];
						
						GatherChildMinMaxs( *node, childIndex, &mins, &maxs );

						if constexpr ( std::is_same_v<func_traits<VisitFunc>::return_type, bool> )
						{
							if ( !VisitCallback( node->leaf( childIndex ), mins, maxs ) )
								return;
						}
						else
						{
							VisitCallback( node->leaf( childIndex ), mins, maxs );
						}
					}
					else
					{
						
						if constexpr ( std::is_same_v<func_traits<VisitFunc>::return_type, bool> )
						{
							if ( !VisitCallback( node->leaf( childIndex ) ) )
								return;
						}
						else
						{
							VisitCallback( node->leaf( childIndex ) );
						}
					}
				}
			}
			else
			{
				for( unsigned childIndex = childCount - 1; childIndex < childCount; --childIndex )
					nodeStack.push( &node->subtree( childIndex ) );
			}
		}
	}

	template <typename Tree, typename VisitFunc>
	static void IterateLeaves( const Tree& root, VisitFunc VisitCallback )
	{
		if constexpr ( func_traits<VisitFunc>::arity == 3 )
		{
			IterateLeaves( const_cast<Tree*>(&root), [&VisitCallback] ( T& leaf, const float ( &mins )[D], const float ( &maxs )[D] )
			{
				VisitCallback( const_cast<const T&>(leaf), mins, maxs );
			} );
		}
		else
		{
			IterateLeaves( const_cast<Tree*>(&root), [&VisitCallback] ( T& leaf )
			{
				VisitCallback( const_cast<const T&>(leaf) );
			} );
		}
	}

	template <typename Tree, typename IntersectFunc, typename QueryFunc>
	static void QueryBase( Tree* root, float epsilon, IntersectFunc &&IntersectCallback, QueryFunc &&QueryCallback )
	{
		std::stack<Tree*> nodeStack;

		nodeStack.push( root );

		while( !nodeStack.empty() )
		{
			Tree* const node = nodeStack.top();
			const unsigned childCount = node->child_count();
			alignas( 16 ) unsigned intersectsChild[MAX_NODES];

			nodeStack.pop();

			IntersectCallback( *node, epsilon, &intersectsChild );

			if( node->leaf_branch() )
			{
				for( unsigned childIndex = 0; childIndex < childCount; ++childIndex )
				{
					if ( intersectsChild[childIndex] )
					{
						if constexpr ( func_traits<QueryFunc>::arity == 3 )
						{
							float mins[D];
							float maxs[D];

							GatherChildMinMaxs( node, childIndex, &mins, &maxs );

							if constexpr ( std::is_same_v<func_traits<QueryFunc>::return_type, bool> )
							{
								if ( !QueryCallback( node->leaf( childIndex ), mins, maxs ) )
									return;
							}
							else
							{
								QueryCallback( node->leaf( childIndex ), mins, maxs );
							}
						}
						else
						{
							if constexpr ( std::is_same_v<func_traits<QueryFunc>::return_type, bool> )
							{
								if ( !QueryCallback( node->leaf( childIndex ) ) )
									return;
							}
							else
							{
								QueryCallback( node->leaf( childIndex ) );	
							}
						}
					}
				}
			}
			else
			{
				for( unsigned childIndex = 0; childIndex < childCount; ++childIndex )
					if( intersectsChild[childIndex] )
						nodeStack.push( &node->subtree( childIndex ) );
			}
		}
	}

	template <typename Tree, typename IntersectFunc, typename QueryFunc>
	static void QueryBase( const Tree& root, float epsilon, IntersectFunc IntersectCallback, QueryFunc QueryCallback )
	{
		if constexpr ( func_traits<QueryFunc>::arity == 3 )
		{
			QueryBase( const_cast<Tree*>(&root), epsilon, std::forward<IntersectFunc>( IntersectCallback ), [&QueryCallback] ( T& leaf, const float ( &mins )[3], const float( &maxs )[3] )
			{
				QueryCallback( const_cast<const T&>(leaf), mins, maxs );
			} );
		}
		else
		{
			QueryBase( const_cast<Tree*>( &root ), epsilon, std::forward<IntersectFunc>(IntersectCallback), [&QueryCallback]( T& leaf )
			{
				QueryCallback( const_cast<const T&>( leaf ) );
			} );
		}
	}

	template <typename Tree, typename QueryFunc>
	static void Query( Tree* root, const float* queryMins, const float* queryMaxs, float epsilon, QueryFunc QueryCallback )
	{
		QueryBase( root, epsilon, [=] ( const Tree &node, float epsilon, unsigned ( *outIntersectsChild )[MAX_NODES] )
		{
			GatherIntersections( node, queryMins, queryMaxs, epsilon, outIntersectsChild );
		}, std::forward<QueryFunc>( QueryCallback ) );
	}

	template <typename Tree, typename QueryFunc>
	static void Query( const Tree& root, const float* queryMins, const float* queryMaxs, float epsilon, QueryFunc QueryCallback )
	{
		QueryBase( root, epsilon, [=] ( const Tree &node, float epsilon, unsigned ( *outIntersectsChild )[MAX_NODES] )
		{
			GatherIntersections( node, queryMins, queryMaxs, epsilon, outIntersectsChild );
		}, std::forward<QueryFunc>( QueryCallback ) );
	}

	template<typename Tree, typename QueryFunc>
	static void SegmentQuery( Tree *root, const float *start, const float *end, float epsilon, QueryFunc QueryCallback )
	{
		float invDir[D];

		for ( unsigned d = 0; d < D; ++d )
		{
			invDir[d] = 1.0f / (end[d] - start[d]);
		}

		QueryBase( root, epsilon, [&] ( const Tree &node, float epsilon, unsigned ( *outIntersectsChild )[MAX_NODES] )
		{
			GatherRayIntersections<true>( node, start, invDir, epsilon, outIntersectsChild );
		}, std::forward<QueryFunc>( QueryCallback ) );
	}

	template<typename Tree, typename QueryFunc>
	static void SegmentQuery( const Tree &root, const float *start, const float *end, float epsilon, QueryFunc QueryCallback )
	{
		float invDir[D];

		for ( unsigned d = 0; d < D; ++d )
		{
			invDir[d] = 1.0f / (end[d] - start[d]);
		}

		QueryBase( root, epsilon, [&] ( const Tree &node, float epsilon, unsigned ( *outIntersectsChild )[MAX_NODES] )
		{
			GatherRayIntersections<true>( node, start, invDir, epsilon, outIntersectsChild );
		}, std::forward<QueryFunc>( QueryCallback ) );
	}

	template<typename Tree, typename QueryFunc>
	static void RayQuery( Tree *root, const float *start, const float *dir, float epsilon, QueryFunc QueryCallback )
	{
		float invDir[D];

		for ( unsigned d = 0; d < D; ++d )
		{
			invDir[d] = 1.0f / dir[d];
		}

		QueryBase( root, epsilon, [&] ( const Tree &node, float epsilon, unsigned ( *outIntersectsChild )[MAX_NODES] )
		{
			GatherRayIntersections<false>( node, start, invDir, epsilon, outIntersectsChild );
		}, std::forward<QueryFunc>( QueryCallback ) );
	}

	template<typename Tree, typename QueryFunc>
	static void RayQuery( const Tree &root, const float *start, const float *dir, float epsilon, QueryFunc QueryCallback )
	{
		float invDir[D];

		for ( unsigned d = 0; d < D; ++d )
		{
			invDir[d] = 1.0f / dir[d];
		}

		QueryBase( root, epsilon, [&] ( const Tree &node, float epsilon, unsigned ( *outIntersectsChild )[MAX_NODES] )
		{
			GatherRayIntersections<false>( node, start, invDir, epsilon, outIntersectsChild );
		}, std::forward<QueryFunc>( QueryCallback ) );
	}

	template<typename Tree, typename QueryFunc, typename = std::enable_if_t<D == 3>>
	static void PlanarQuery( Tree *root, const float ( &plane )[4], float epsilon, QueryFunc QueryCallback )
	{
		QueryBase( root, epsilon, [&] ( const Tree &node, float epsilon, unsigned ( *outIntersectsChild )[MAX_NODES] )
		{
			GatherPlanarIntersections( node, plane, epsilon, outIntersectsChild );
		}, std::forward<QueryFunc>( QueryCallback ) );
	}

	template<typename Tree, typename QueryFunc, typename = std::enable_if_t<D == 3>>
	static void PlanarQuery( const Tree &root, const float ( &plane )[4], float epsilon, QueryFunc QueryCallback )
	{
		QueryBase( root, epsilon, [&] ( const Tree &node, float epsilon, unsigned ( *outIntersectsChild )[MAX_NODES] )
		{
			GatherPlanarIntersections( node, plane, epsilon, outIntersectsChild );
		}, std::forward<QueryFunc>( QueryCallback ) );
	}

	template <typename Tree>
	static void ValidateTree_r( const Tree& tree, const float ( &treeMins )[D], const float ( &treeMaxs )[D] )
	{
		const size_t childCount = tree.child_count();
		sanity( childCount <= Tree::max_children );

		if( childCount == 0 )
			return;

		for( size_t d = 0; d < D; ++d )
		{
			const float* const childMins = tree.child_mins( d );
			const float* const childMaxs = tree.child_maxs( d );

			sanity( treeMins[d] <= treeMaxs[d] );

			for( size_t childIndex = 0; childIndex < childCount; ++childIndex )
			{
				sanity( childMins[childIndex] >= treeMins[d] );
				sanity( childMaxs[childIndex] <= treeMaxs[d] );
			}
		}

		if( !tree.leaf_branch() )
		{
			for( size_t childIndex = 0; childIndex < childCount; ++childIndex )
			{
				float childMins[D];
				float childMaxs[D];

				GatherChildMinMaxs( tree, static_cast<unsigned>( childIndex ), &childMins, &childMaxs );
				ValidateTree_r( tree.subtree( static_cast<unsigned>( childIndex ) ), childMins, childMaxs );
			}
		}
	}

	template <typename Tree, typename TransformFunc>
	static void Transform_r( Tree* parent, unsigned myIndex, TransformFunc TransformCallback )
	{
		auto TransformLeaf = [&TransformCallback]( Tree* tree, unsigned index )
		{
			float childMins[D];
			float childMaxs[D];

			GatherChildMinMaxs( *tree, index, &childMins, &childMaxs );
			if( TransformCallback( tree->leaf( index ), childMins, childMaxs ) )
			{
				for( size_t d = 0; d < D; ++d )
				{
					assertfmt( childMins[d] <= childMaxs[d], "Transform callback returned invalid bounds, %f <= %f", childMins[d], childMaxs[d] );
					tree->child_mins( d )[index] = childMins[d];
					tree->child_maxs( d )[index] = childMaxs[d];
				}
			}
		};

		if( parent->leaf_branch() )
			TransformLeaf( parent, myIndex ); // If the root only has leaves, we'll hit this. No parent mins/maxs to update
		else
		{
			Tree* const me = &parent->subtree( myIndex );
			const unsigned myChildCount = me->child_count();

			if( me->leaf_branch() )
			{
				for( unsigned childIndex = 0; childIndex < myChildCount; ++childIndex )
					TransformLeaf( me, childIndex );
			}
			else
			{
				for( unsigned childIndex = 0; childIndex < myChildCount; ++childIndex )
					Transform_r( me, childIndex, std::forward<TransformFunc>( TransformCallback ) );
			}

			for( size_t d = 0; d < D; ++d )
				AxialMinMax( me->child_mins( d ), me->child_maxs( d ), myChildCount, &parent->child_mins( d )[myIndex], &parent->child_maxs( d )[myIndex] );
		}
	}
};
}
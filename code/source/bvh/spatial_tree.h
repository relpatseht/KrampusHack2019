#pragma once

#include "spatial_tree_common.h"
#include "box_partitioner.h"
#include "Assert.h"
#include <iterator>

#ifndef DEBUG_CONTAINERS
#if defined( DEBUG ) || defined( _DEBUG )
#define DEBUG_CONTAINERS IN_USE
#else //#if defined(DEBUG) || defined(_DEBUG)
#define DEBUG_CONTAINERS NOT_IN_USE
#endif //#else //#if defined(DEBUG) || defined(_DEBUG)
#endif //#ifndef DEBUG_CONTAINERS

template <typename T, size_t D = 3, size_t MAX_NODES = 16>
class spatial_tree
{
private:
	typedef spatial_tree_common::TreeInterface<T, D, MAX_NODES> tree_op;
	typedef typename std::aligned_storage<sizeof( T ), alignof( T )>::type LeafStorage;

	template <typename U, size_t C, size_t MN>
	friend class spatial_tree;

	friend struct tree_op;

	alignas( 16 ) float mins[D][MAX_NODES];
	alignas( 16 ) float maxs[D][MAX_NODES];

	unsigned char containsLeaves;
	unsigned char childCount;

	union
	{
		spatial_tree* branches;
		LeafStorage* leaves;
		void* data;
	} children;

	template <typename T>
	static T* get_temporary_address( T&& x )
	{
		return &x;
	}

	template <typename T>
	static const T* get_temporary_address( const T& x )
	{
		return &x;
	}

public:
	typedef T value_type;
	static const size_t dimensions = D;
	static const size_t max_children = MAX_NODES;

	enum 
	{
		MAX_VECTOR_ALIGNMENT = 64,
	};

	spatial_tree()
		: mins{}
		, maxs{}
		, containsLeaves( 0 )
		, childCount( 0 )
		, children( { nullptr } )
	{
	}

	template <typename Iter, typename BoundsGen>
	spatial_tree( Iter start, Iter end, BoundsGen BoundsGenerator )
		: spatial_tree()
	{
		using namespace spatial_tree_common;
		typedef typename std::iterator_traits<Iter>::reference IterRef;
		static const bool IterIsRValue = std::is_rvalue_reference_v<IterRef>;
		typedef typename std::conditional<IterIsRValue, T, const T>::type LT;
		
		const unsigned leafCount = static_cast<unsigned>( end - start );

		if( leafCount == 0 )
			return;

		size_t workMemSize;
		void* workMem = AllocWorkMem( leafCount, &workMemSize );
		void* workMemPtr = workMem;

		float* leafMins[D];
		float* leafMaxs[D];
		LT** leafPtrs = WorkMemAlloc<LT*>( leafCount, &workMemPtr );

		for( unsigned d = 0; d < D; ++d )
		{
			leafMins[d] = WorkMemAlloc<float>( leafCount, &workMemPtr );
			leafMaxs[d] = WorkMemAlloc<float>( leafCount, &workMemPtr );
		}

		for( unsigned leafIndex = 0; start != end; ++start, ++leafIndex )
		{
			float curMins[D];
			float curMaxs[D];

			leafPtrs[leafIndex] = get_temporary_address( *start );
			BoundsGenerator( *start, curMins, curMaxs );

			for( unsigned d = 0; d < D; ++d )
			{
				assertfmt( curMins[d] <= curMaxs[d], "Object has invalid bounds" );
				leafMins[d][leafIndex] = curMins[d];
				leafMaxs[d][leafIndex] = curMaxs[d];
			}
		}

		LoadNode( this, leafPtrs, leafMins, leafMaxs, leafCount, workMemPtr );

		size_t workMemUsed = static_cast<size_t>( (char*)workMemPtr - (char*)workMem );
		assert( workMemUsed <= workMemSize );
		FreeWorkMem( workMem );
	}

	spatial_tree( spatial_tree&& rhs )
		: childCount( rhs.childCount )
		, containsLeaves( rhs.containsLeaves )
	{
		rhs.childCount = 0;

		children.data = rhs.children.data;
		rhs.children.data = nullptr;

		for( size_t d = 0; d < D; ++d )
		{
			std::memcpy( mins[d], rhs.mins[d], sizeof( float ) * childCount );
			std::memcpy( maxs[d], rhs.maxs[d], sizeof( float ) * childCount );
		}
	}

	spatial_tree( const spatial_tree& rhs )
		: childCount( 0 )
		, children( { 0 } )
	{
		*this = rhs;
	}

	template <typename Tree>
	spatial_tree( const Tree& rhs )
		: childCount( 0 )
		, children( { 0 } )
	{
		*this = rhs;
	}

	~spatial_tree()
	{
		DestructAndFreeChildren();
	}

	spatial_tree& operator=( spatial_tree&& rhs )
	{
		if( this != &rhs )
		{
			DestructAndFreeChildren();

			childCount = rhs.childCount;
			containsLeaves = rhs.containsLeaves;
			children.data = rhs.children.data;

			for( size_t d = 0; d < D; ++d )
			{
				std::memcpy( mins[d], rhs.mins[d], sizeof( float ) * childCount );
				std::memcpy( maxs[d], rhs.maxs[d], sizeof( float ) * childCount );
			}

			rhs.childCount = 0;
			rhs.children.data = nullptr;
		}

		return *this;
	}

	spatial_tree& operator=( const spatial_tree& rhs )
	{
		return operator=<spatial_tree>( rhs );
	}

	template <typename Tree>
	spatial_tree& operator=( const Tree& rhs )
	{
		if( reinterpret_cast<void*>( this ) != reinterpret_cast<const void*>( &rhs ) )
		{
			DestructAndFreeChildren();

			assert( rhs.child_count() <= 255 );
			childCount = static_cast<unsigned char>( rhs.child_count() );
			containsLeaves = rhs.leaf_branch();

			if( containsLeaves )
			{
				AllocateLeaves( childCount );
				for( unsigned childIndex = 0; childIndex < childCount; ++childIndex )
					ConstructLeaf( childIndex, rhs.leaf( childIndex ) );
			}
			else
			{
				AllocateBranches( childCount );
				for( unsigned childIndex = 0; childIndex < childCount; ++childIndex )
				{
					spatial_tree* const child = &children.branches[childIndex];

					*child = rhs.subtree( childIndex );
				}
			}

			for( size_t d = 0; d < D; ++d )
			{
				std::memcpy( mins[d], rhs.child_mins( d ), sizeof( float ) * childCount );
				std::memcpy( maxs[d], rhs.child_maxs( d ), sizeof( float ) * childCount );
			}
		}

		return *this;
	}

	__forceinline void clear()
	{
		DestructAndFreeChildren();
	}

	__forceinline bool empty() const
	{
		return childCount == 0;
	}

	__forceinline unsigned child_count() const
	{
		return childCount;
	}

	__forceinline bool leaf_branch() const
	{
		return containsLeaves;
	}

	void insert( const T& val, const float ( &newMins )[D], const float ( &newMaxs )[D] )
	{
		const float newSurfaceArea = spatial_tree_common::SurfaceArea( newMins, newMaxs );

		if( !children.data )
		{
			containsLeaves = 1;
			AllocateLeaves( 1 );
		}

		if( !Insert( newMins, newMaxs, newSurfaceArea, &val ) )
			Split( newMins, newMaxs, &val );
	}

	template <typename Iter, typename BoundsGen>
	void insert( Iter valStart, Iter valEnd, BoundsGen Bounds )
	{
		spatial_tree bulkTree( valStart, valEnd, Bounds );

		merge( std::move( bulkTree ) );
	}

	template <typename Tree>
	void merge( const Tree& rhs )
	{
		spatial_tree copy{ rhs };
		merge( std::move( copy ) );
	}

	void merge( const spatial_tree& rhs )
	{
		spatial_tree copy{ rhs };
		merge( std::move( copy ) );
	}

	void merge( spatial_tree&& rhs )
	{
		if( childCount == 0 )
		{
			*this = std::move( rhs );
		}
		else if( rhs.childCount != 0 )
		{
			float rhsMins[D];
			float rhsMaxs[D];
			float myMins[D];
			float myMaxs[D];

			tree_op::TreeMinMax( rhs, &rhsMins, &rhsMaxs );
			tree_op::TreeMinMax( *this, &myMins, &myMaxs );
			const float rhsSurfaceArea = spatial_tree_common::SurfaceArea( rhsMins, rhsMaxs );
			const float mySurfaceArea = spatial_tree_common::SurfaceArea( myMins, myMaxs );

			if( mySurfaceArea >= rhsSurfaceArea )
				Merge( rhsMins, rhsMaxs, rhsSurfaceArea, &rhs );
			else
			{
				rhs.Merge( myMins, myMaxs, mySurfaceArea, this );

				*this = std::move( rhs );
			}
		}
	}

	// Visitor is either R(const T&) or R(const T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template <typename Visitor>
	void iterate( Visitor VisitFunc ) const
	{
		tree_op::IterateLeaves( *this, std::forward<Visitor>( VisitFunc ) );
	}
	
	// Visitor is either R(T&) or R(T&, const float (&mins)[D], const float (&maxs)[D])
	template <typename Visitor>
	void iterate( Visitor VisitFunc )
	{
		tree_op::IterateLeaves( this, std::forward<Visitor>( VisitFunc ) );
	}
	
	// Query is either R(const T&) or R(const T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template <typename Query>
	void query( const float* queryMins, const float* queryMaxs, Query QueryFunc ) const
	{
		tree_op::Query( *this, queryMins, queryMaxs, 0.0f, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(T&) or R(T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template <typename Query>
	void query( const float* queryMins, const float* queryMaxs, Query QueryFunc )
	{
		tree_op::Query( this, queryMins, queryMaxs, 0.0f, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(const T&) or R(const T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template <typename Query>
	void query_epsilon( const float* queryMins, const float* queryMaxs, float epsilon, Query QueryFunc ) const
	{
		tree_op::Query( *this, queryMins, queryMaxs, epsilon, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(T&) or R(T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template <typename Query>
	void query_epsilon( const float* queryMins, const float* queryMaxs, float epsilon, Query QueryFunc )
	{
		tree_op::Query( this, queryMins, queryMaxs, epsilon, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(T&) or R(T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template<typename Query>
	void segment_query( const float* start, const float* end, Query QueryFunc )
	{
		tree_op::SegmentQuery( this, start, end, 0.0f, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(const T&) or R(const T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template<typename Query>
	void segment_query( const float* start, const float* end, Query QueryFunc ) const
	{
		tree_op::SegmentQuery( *this, start, end, 0.0f, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(T&) or R(T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template<typename Query>
	void segment_query_epsilon( const float* start, const float* end, float epsilon, Query QueryFunc )
	{
		tree_op::SegmentQuery( this, start, end, epsilon, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(const T&) or R(const T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template<typename Query>
	void segment_query_epsilon( const float* start, const float* end, float epsilon, Query QueryFunc ) const
	{
		tree_op::SegmentQuery( *this, start, end, epsilon, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(T&) or R(T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template<typename Query>
	void ray_query( const float* start, const float* dir, Query QueryFunc )
	{
		tree_op::RayQuery( this, start, dir, 0.0f, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(const T&) or R(const T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template<typename Query>
	void ray_query( const float* start, const float* dir, Query QueryFunc ) const
	{
		tree_op::RayQuery( *this, start, dir, 0.0f, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(T&) or R(T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template<typename Query>
	void ray_query_epsilon( const float* start, const float* dir, float epsilon, Query QueryFunc )
	{
		tree_op::RayQuery( this, start, dir, epsilon, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(const T&) or R(const T&, const float (&mins)[D], const float (&maxs)[D]). If R is bool, returning false early outs.
	template<typename Query>
	void ray_query_epsilon( const float* start, const float* dir, float epsilon, Query QueryFunc ) const
	{
		tree_op::RayQuery( *this, start, dir, epsilon, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(T&) or R(T&, const float (&mins)[3], const float (&maxs)[3]). If R is bool, returning false early outs.
	template<typename Query, typename = std::enable_if_t<D == 3>>
	void planar_query( const float ( &plane )[4], Query QueryFunc )
	{
		tree_op::PlanarQuery( this, plane, 0.0f, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(const T&) or R(const T&, const float (&mins)[3], const float (&maxs)[3]). If R is bool, returning false early outs.
	template<typename Query, typename = std::enable_if_t<D == 3>>
	void planar_query( const float ( &plane )[4], Query QueryFunc ) const
	{
		tree_op::PlanarQuery( *this, plane, 0.0f, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(T&) or R(T&, const float (&mins)[3], const float (&maxs)[3]). If R is bool, returning false early outs.
	template<typename Query, typename = std::enable_if_t<D == 3>>
	void planar_query_epsilon( const float ( &plane )[4], float epsilon, Query QueryFunc )
	{
		tree_op::PlanarQuery( this, plane, epsilon, std::forward<Query>( QueryFunc ) );
	}
	
	// Query is either R(const T&) or R(const T&, const float (&mins)[3], const float (&maxs)[3]). If R is bool, returning false early outs.
	template<typename Query, typename = std::enable_if_t<D == 3>>
	void planar_query_epsilon( const float ( &plane )[4], float epsilon, Query QueryFunc ) const
	{
		tree_op::PlanarQuery( *this, plane, epsilon, std::forward<Query>( QueryFunc ) );
	}

	// Clipper is either R(T&, RHSTree::value_type&) or R(T&, RHSTree::value_type&, const float (&lhsMins)[3], const float (&lhsMaxs)[3], const float (&rhsMins)[3], const float (&rhsMaxs)[3]). If R is bool, returning false early outs.
	template <typename RHSTree, typename Clipper>
	void clip_epsilon( RHSTree& rhs, float epsilon, Clipper ClipFunc )
	{
		using namespace spatial_tree_common;
		float lhsMins[D];
		float lhsMaxs[D];
		float rhsMins[D];
		float rhsMaxs[D];

		tree_op::TreeMinMax( *this, lhsMins, lhsMaxs );
		tree_op::TreeMinMax( rhs, rhsMins, rhsMaxs );

		tree_op::Clip( this, rhs, epsilon, lhsMins, lhsMaxs, rhsMins, rhsMaxs, std::forward<Clipper>( ClipFunc ) );
	}
	
	// Clipper is either R(const T&, RHSTree::value_type&) or R(const T&, RHSTree::value_type&, const float (&lhsMins)[3], const float (&lhsMaxs)[3], const float (&rhsMins)[3], const float (&rhsMaxs)[3]). If R is bool, returning false early outs.
	template <typename RHSTree, typename Clipper>
	void clip_epsilon( RHSTree& rhs, float epsilon, Clipper ClipFunc ) const
	{
		using namespace spatial_tree_common;
		float lhsMins[D];
		float lhsMaxs[D];
		float rhsMins[D];
		float rhsMaxs[D];

		tree_op::TreeMinMax( *this, lhsMins, lhsMaxs );
		tree_op::TreeMinMax( rhs, rhsMins, rhsMaxs );

		tree_op::Clip( *this, rhs, epsilon, lhsMins, lhsMaxs, rhsMins, rhsMaxs, std::forward<Clipper>( ClipFunc ) );
	}

	// Clipper is either R(T&, RHSTree::value_type&) or R(T&, RHSTree::value_type&, const float (&lhsMins)[3], const float (&lhsMaxs)[3], const float (&rhsMins)[3], const float (&rhsMaxs)[3]). If R is bool, returning false early outs.
	template <typename RHSTree, typename Clipper>
	void clip( RHSTree& rhs, Clipper ClipFunc )
	{
		clip_epsilon( rhs, 0.0f, std::forward<Clipper>( ClipFunc ) );
	}

	// Clipper is either R(const T&, RHSTree::value_type&) or R(const T&, RHSTree::value_type&, const float (&lhsMins)[3], const float (&lhsMaxs)[3], const float (&rhsMins)[3], const float (&rhsMaxs)[3]). If R is bool, returning false early outs.
	template <typename RHSTree, typename Clipper>
	void clip( RHSTree& rhs, Clipper ClipFunc ) const
	{
		clip_epsilon( rhs, 0.0f, std::forward<Clipper>( ClipFunc ) );
	}

	// Clipper is either R(T&, T&) or R(T&, T&, const float (&lhsMins)[3], const float (&lhsMaxs)[3], const float (&rhsMins)[3], const float (&rhsMaxs)[3]). If R is bool, returning false early outs.
	template <typename Clipper>
	void self_clip_epsilon( float epsilon, Clipper ClipFunc )
	{
		using namespace spatial_tree_common;
		float rootMins[D];
		float rootMaxs[D];

		tree_op::TreeMinMax( *this, rootMins, rootMaxs );

		tree_op::Clip( this, *this, epsilon, rootMins, rootMaxs, rootMins, rootMaxs, std::forward<Clipper>( ClipFunc ) );
	}
	
	// Clipper is either R(const T&, const T&) or R(const T&, const T&, const float (&lhsMins)[3], const float (&lhsMaxs)[3], const float (&rhsMins)[3], const float (&rhsMaxs)[3]). If R is bool, returning false early outs.
	template <typename Clipper>
	void self_clip_epsilon( float epsilon, Clipper ClipFunc ) const
	{
		using namespace spatial_tree_common;
		float rootMins[D];
		float rootMaxs[D];
		
		tree_op::TreeMinMax( *this, rootMins, rootMaxs );

		tree_op::Clip( *this, *this, epsilon, rootMins, rootMaxs, rootMins, rootMaxs, std::forward<Clipper>( ClipFunc ) );
	}

	// Clipper is either R(T&, T&) or R(T&, T&, const float (&lhsMins)[3], const float (&lhsMaxs)[3], const float (&rhsMins)[3], const float (&rhsMaxs)[3]). If R is bool, returning false early outs.
	template <typename Clipper>
	void self_clip( Clipper ClipFunc )
	{
		self_clip_epsilon( 0.0f, std::forward<Clipper>( ClipFunc ) );
	}

	// Clipper is either R(const T&, const T&) or R(const T&, const T&, const float (&lhsMins)[3], const float (&lhsMaxs)[3], const float (&rhsMins)[3], const float (&rhsMaxs)[3]). If R is bool, returning false early outs.
	template <typename Clipper>
	void self_clip( Clipper ClipFunc ) const
	{
		self_clip_epsilon( 0.0f, std::forward<Clipper>( ClipFunc ) );
	}

	// TransformFunc is bool(T&, float (*inoutMins)[D], float (*inoutMaxs)[D]) (true to apply, false to ignore)
	template <typename TransformFunc>
	void transform( TransformFunc Callback )
	{
		for( unsigned childIndex = 0; childIndex < childCount; ++childIndex )
			tree_op::Transform_r( this, childIndex, std::forward<TransformFunc>( Callback ) );
	}

	void bounds( float* outMins, float* outMaxs ) const
	{
		tree_op::TreeMinMax( *this, outMins, outMaxs );
	}

	size_t size() const
	{
		return tree_op::Size( *this );
	}

	__forceinline const spatial_tree& subtree( unsigned childIndex ) const
	{
		return children.branches[childIndex];
	}

	__forceinline const T& leaf( unsigned childIndex ) const
	{
		return reinterpret_cast<const T&>( children.leaves[childIndex] );
	}

	__forceinline spatial_tree& subtree( unsigned childIndex )
	{
		return children.branches[childIndex];
	}

	__forceinline T& leaf( unsigned childIndex )
	{
		return reinterpret_cast<T&>( children.leaves[childIndex] );
	}

	__forceinline const float* child_mins( size_t d ) const
	{
		return mins[d];
	}

	__forceinline const float* child_maxs( size_t d ) const
	{
		return maxs[d];
	}

private:
	__forceinline void AllocateLeaves( unsigned /*count*/ )
	{
		children.leaves = new LeafStorage[MAX_NODES];
	}

	__forceinline void AllocateBranches( unsigned /*count*/ )
	{
		children.branches = new spatial_tree[MAX_NODES];
	}

	void FreeChildren()
	{
		if( containsLeaves )
			delete[] children.leaves;
		else
			delete[] children.branches;

		childCount = 0;
		children.data = nullptr;
	}

	void DestructAndFreeChildren()
	{
		if( containsLeaves )
		{
			for( unsigned childIndex = 0; childIndex < childCount; ++childIndex )
				DestructLeaf( childIndex );
		}

		FreeChildren();
	}

	template <typename... Args>
	__forceinline void ConstructLeaf( unsigned index, Args&& ... args )
	{
		new( children.leaves + index ) T( std::forward<Args>( args )... );
	}

	__forceinline void DestructLeaf( unsigned index )
	{
		reinterpret_cast<T*>( children.leaves + index )->~T();
	}

	__forceinline float* child_mins( size_t d )
	{
		return mins[d];
	}

	__forceinline float* child_maxs( size_t d )
	{
		return maxs[d];
	}

	static spatial_tree BranchMerge( spatial_tree* lhs, const float ( &rhsMins )[D], const float ( &rhsMaxs )[D], spatial_tree* rhs )
	{
		using namespace spatial_tree_common;
		spatial_tree newTree;

		newTree.AllocateBranches( 2 );
		newTree.childCount = 2;
		newTree.containsLeaves = 0;

		for( size_t d = 0; d < D; ++d )
		{
			newTree.mins[d][0] = rhsMins[d];
			newTree.maxs[d][0] = rhsMaxs[d];
			AxialMinMax( lhs->mins[d], lhs->maxs[d], lhs->childCount, &newTree.mins[d][1], &newTree.maxs[d][1] );
		}

		newTree.children.branches[0] = std::move( *rhs );
		newTree.children.branches[1] = std::move( *lhs );

		return newTree;
	}

	void Merge( const float ( &rhsMins )[D], const float ( &rhsMaxs )[D], float rhsSurfaceArea, spatial_tree* rhs )
	{
		using namespace spatial_tree_common;

		if( containsLeaves )
		{
			if( rhs->leaf_branch() && childCount + rhs->child_count() <= MAX_NODES )
			{
				for( uint rhsChildIndex = 0; rhsChildIndex < rhs->child_count(); ++rhsChildIndex )
				{
					ConstructLeaf( childCount, std::move( rhs->leaf( rhsChildIndex ) ) );
					for( size_t d = 0; d < D; ++d )
					{
						mins[d][childCount] = rhs->child_mins( d )[rhsChildIndex];
						maxs[d][childCount] = rhs->child_maxs( d )[rhsChildIndex];
					}

					++childCount;
				}

				rhs->DestructAndFreeChildren();
			}
			else
				*this = BranchMerge( this, rhsMins, rhsMaxs, rhs );
		}
		else
		{
			static const float minMergeOverlapPercent = 0.5f;
			const float* childMins[D];
			const float* childMaxs[D];
			alignas( 16 ) float childSurfaceAreas[MAX_NODES];
			alignas( 16 ) float overlapPercent[MAX_NODES];
			unsigned mergeIndex;

			for( size_t d = 0; d < D; ++d )
			{
				childMins[d] = mins[d];
				childMaxs[d] = maxs[d];
			}

			SurfaceAreas( childCount, childMins, childMaxs, &childSurfaceAreas );
			PercentOverlap( rhsSurfaceArea, rhsMins, rhsMaxs, childCount, childSurfaceAreas, childMins, childMaxs, &overlapPercent );

			const float* const maxOverlapPtr = std::max_element( overlapPercent, overlapPercent + childCount );
			if( *maxOverlapPtr > minMergeOverlapPercent )
				mergeIndex = static_cast<unsigned>( maxOverlapPtr - overlapPercent );
			else if( childCount < MAX_NODES )
			{
				spatial_tree* const newBranch = &children.branches[childCount];

				*newBranch = std::move( *rhs );

				for( size_t d = 0; d < D; ++d )
				{
					mins[d][childCount] = rhsMins[d];
					maxs[d][childCount] = rhsMaxs[d];
				}

				++childCount;

				return;
			}
			else if( *maxOverlapPtr == 0.0f )
			{
				alignas( 16 ) float distances[MAX_NODES];

				BoxDistancesSqr( rhsMins, rhsMaxs, childCount, childMins, childMaxs, &distances );
				const float* const closestDistPtr = std::min_element( distances, distances + childCount );
				mergeIndex = static_cast<unsigned>( closestDistPtr - distances );
			}
			else
				mergeIndex = static_cast<unsigned>( maxOverlapPtr - overlapPercent );

			const float mergeSA = childSurfaceAreas[mergeIndex];
			spatial_tree* const mergeBranch = &children.branches[mergeIndex];

			if( mergeSA > rhsSurfaceArea )
				mergeBranch->Merge( rhsMins, rhsMaxs, rhsSurfaceArea, rhs );
			else
			{
				float mergeMins[D];
				float mergeMaxs[D];
				for( size_t d = 0; d < D; ++d )
				{
					mergeMins[d] = childMins[d][mergeIndex];
					mergeMaxs[d] = childMaxs[d][mergeIndex];
				}

				rhs->Merge( mergeMins, mergeMaxs, mergeSA, mergeBranch );
				*mergeBranch = std::move( *rhs );
			}

			for( size_t d = 0; d < D; ++d )
			{
				mins[d][mergeIndex] = std::min( mins[d][mergeIndex], rhsMins[d] );
				maxs[d][mergeIndex] = std::max( maxs[d][mergeIndex], rhsMaxs[d] );
			}
		}
	}

	template <typename LT>
	bool Insert( const float ( &nodeMins )[D], const float ( &nodeMaxs )[D], float nodeSurfaceArea, LT* val )
	{
		using namespace spatial_tree_common;

		if( containsLeaves )
		{
			if( childCount >= MAX_NODES )
				return false;

			ConstructLeaf( childCount, std::move( *val ) );
			for( size_t d = 0; d < D; ++d )
			{
				mins[d][childCount] = nodeMins[d];
				maxs[d][childCount] = nodeMaxs[d];
			}

			++childCount;
		}
		else
		{
			const float* childMins[D];
			const float* childMaxs[D];
			alignas( 16 ) float childSurfaceAreas[MAX_NODES];
			alignas( 16 ) float overlapPercent[MAX_NODES];

			for( size_t d = 0; d < D; ++d )
			{
				childMins[d] = mins[d];
				childMaxs[d] = maxs[d];
			}

			SurfaceAreas( childCount, childMins, childMaxs, &childSurfaceAreas );
			PercentOverlap( nodeSurfaceArea, nodeMins, nodeMaxs, childCount, childSurfaceAreas, childMins, childMaxs, &overlapPercent );

			const float* const maxOverlapPtr = std::max_element( overlapPercent, overlapPercent + childCount );
			if( *maxOverlapPtr > 0.0f )
			{
				const unsigned overlapIndex = static_cast<unsigned>( maxOverlapPtr - overlapPercent );
				const bool inserted = children.branches[overlapIndex].Insert( nodeMins, nodeMaxs, nodeSurfaceArea, val );

				if( inserted )
				{
					for( size_t d = 0; d < D; ++d )
					{
						mins[d][overlapIndex] = std::min( mins[d][overlapIndex], nodeMins[d] );
						maxs[d][overlapIndex] = std::max( maxs[d][overlapIndex], nodeMaxs[d] );
					}
				}
				else
				{
					Split( nodeMins, nodeMaxs, val );
				}
			}
			else
			{
				if( childCount >= MAX_NODES )
					return false;
				else
				{
					spatial_tree* const newBranch = &children.branches[childCount];

					newBranch->containsLeaves = 1;
					newBranch->childCount = 1;
					newBranch->AllocateLeaves( 1 );
					newBranch->ConstructLeaf( 0, std::move( *val ) );
					for( size_t d = 0; d < D; ++d )
					{
						newBranch->mins[d][0] = nodeMins[d];
						newBranch->maxs[d][0] = nodeMaxs[d];
						mins[d][childCount] = nodeMins[d];
						maxs[d][childCount] = nodeMaxs[d];
					}

					++childCount;
				}
			}
		}

		return true;
	}

	static T* MoveOrCopy( LeafStorage*, T* val )
	{
		return val;
	}

	static T* MoveOrCopy( LeafStorage* storage, const T* val )
	{
		return new( storage ) T( *val );
	}

	template <typename LT>
	void Split( const float ( &newLeafMins )[D], const float ( &newLeafMaxs )[D], LT* val )
	{
		using namespace spatial_tree_common;

		LeafStorage newLeafStorage;
		const size_t leafCount = size() + 1; // +1 for our tmpLeaf

		size_t workMemSize;
		void* const workMem = AllocWorkMem( static_cast<unsigned>( leafCount ), &workMemSize );
		void* workMemPtr = workMem;
		T** leafPtrs = WorkMemAlloc<T*>( leafCount, &workMemPtr );
		float* leafMins[D];
		float* leafMaxs[D];
		size_t curLeafIndex;
		std::stack<spatial_tree*> nodeStack;

		leafPtrs[0] = MoveOrCopy( &newLeafStorage, val );
		for( unsigned d = 0; d < D; ++d )
		{
			leafMins[d] = WorkMemAlloc<float>( leafCount, &workMemPtr );
			leafMaxs[d] = WorkMemAlloc<float>( leafCount, &workMemPtr );
			leafMins[d][0] = newLeafMins[d];
			leafMaxs[d][0] = newLeafMaxs[d];
		}

		// Gather all leaf pointers
		curLeafIndex = 1;
		nodeStack.push( this );
		while( !nodeStack.empty() )
		{
			spatial_tree* const node = nodeStack.top();
			const unsigned nodeChildCount = node->childCount;

			nodeStack.pop();
			if( node->containsLeaves )
			{
				for( unsigned leafIndex = 0; leafIndex < nodeChildCount; ++leafIndex )
					leafPtrs[curLeafIndex + leafIndex] = &node->leaf( leafIndex );

				for( size_t d = 0; d < D; ++d )
				{
					std::memcpy( &leafMins[d][curLeafIndex], node->mins[d], sizeof( float ) * nodeChildCount );
					std::memcpy( &leafMaxs[d][curLeafIndex], node->maxs[d], sizeof( float ) * nodeChildCount );
				}

				curLeafIndex += nodeChildCount;
			}
			else
			{
				for( unsigned childIndex = 0; childIndex < nodeChildCount; ++childIndex )
					nodeStack.push( &node->subtree( childIndex ) );
			}
		}

		// Build a new tree out of all the leafs
		spatial_tree newTree;
		newTree.LoadNode( &newTree, leafPtrs, leafMins, leafMaxs, static_cast<unsigned>( leafCount ), workMemPtr );

		// Replace this node with newTree
		*this = std::move( newTree );

		size_t workMemUsed = static_cast<size_t>( (char*)workMemPtr - (char*)workMem );
		assert( workMemUsed <= workMemSize );
		FreeWorkMem( workMem );
	}

	template <typename LT>
	static void LoadLeafs( spatial_tree* node, LT** leafPtrs, float * ( &leafMins )[D], float * ( &leafMaxs )[D], unsigned leafCount )
	{
		node->containsLeaves = 1;
		node->childCount = static_cast<uint8_t>( leafCount );

		node->AllocateLeaves( leafCount );
		for( unsigned leafIndex = 0; leafIndex < leafCount; ++leafIndex )
			node->ConstructLeaf( leafIndex, std::move( *leafPtrs[leafIndex] ) );

		for( unsigned d = 0; d < D; ++d )
		{
			const float* const leafMinsD = leafMins[d];
			const float* const leafMaxsD = leafMaxs[d];
			float* const childMinsD = node->mins[d];
			float* const childMaxsD = node->maxs[d];

			for( unsigned leafIndex = 0; leafIndex < leafCount; ++leafIndex )
			{
				childMinsD[leafIndex] = leafMinsD[leafIndex];
				childMaxsD[leafIndex] = leafMaxsD[leafIndex];
			}
		}
	}

	template <typename LT>
	static void LoadBranches( spatial_tree* node, LT** leafPtrs, float * ( &leafMins )[D], float * ( &leafMaxs )[D], unsigned leafCount, void* workMem )
	{
		using namespace spatial_tree_common;
		unsigned partitions[MAX_NODES];
		void* workMemPtr = workMem;
		unsigned* const indices = WorkMemAlloc<unsigned>( leafCount + MAX_VECTOR_ALIGNMENT / sizeof( unsigned ), &workMemPtr, MAX_VECTOR_ALIGNMENT );
		void* const partWorkMem = workMemPtr;

		PartitionBoxes( MAX_NODES, leafMins, leafMaxs, leafCount, indices, partitions, partWorkMem );

		bool* const swapped = WorkMemAlloc<bool>( leafCount, &workMemPtr );
		std::memset( swapped, 0, sizeof( bool ) * leafCount );

		for( unsigned leafIndex = 0; leafIndex < leafCount; ++leafIndex )
		{
			if( swapped[leafIndex] )
				continue;

			swapped[leafIndex] = true;
			unsigned prevLeafIndex = leafIndex;
			unsigned swapLeafIndex = indices[leafIndex];
			while( leafIndex != swapLeafIndex )
			{
				std::swap( leafPtrs[prevLeafIndex], leafPtrs[swapLeafIndex] );

				swapped[swapLeafIndex] = true;
				prevLeafIndex = swapLeafIndex;
				swapLeafIndex = indices[swapLeafIndex];
			}
		}

		unsigned leafBegin = 0;
		unsigned childCount = 0;
		for( unsigned partitionIndex = 0; partitionIndex < MAX_NODES; ++partitionIndex )
		{
			const unsigned leafEnd = partitions[partitionIndex];

			if( leafBegin != leafEnd )
			{
				++childCount;
				leafBegin = leafEnd;
			}
		}

		if( childCount <= 1 )
		{
			const unsigned leavesPerChild = static_cast<unsigned>( std::max( MAX_NODES, ( leafCount + ( MAX_NODES - 1 ) ) / MAX_NODES ) );
			childCount = static_cast<unsigned>( std::min( MAX_NODES, ( leafCount + ( MAX_NODES - 1 ) ) / MAX_NODES ) );

			for( unsigned partitionIndex = 0; partitionIndex < MAX_NODES; ++partitionIndex )
				partitions[partitionIndex] = std::min( leavesPerChild * ( partitionIndex + 1 ), leafCount );
		}

		node->containsLeaves = 0;
		node->AllocateBranches( childCount );
		node->childCount = static_cast<uint8_t>( childCount );

		leafBegin = 0;
		unsigned childIndex = 0;
		for( size_t partitionIndex = 0; partitionIndex < MAX_NODES; ++partitionIndex )
		{
			const unsigned leafEnd = partitions[partitionIndex];

			if( leafEnd != leafBegin )
			{
				const unsigned childLeafCount = leafEnd - leafBegin;
				float* childMinPtrs[D], *childMaxPtrs[D];

				for( size_t d = 0; d < D; ++d )
				{
					childMinPtrs[d] = leafMins[d] + leafBegin;
					childMaxPtrs[d] = leafMaxs[d] + leafBegin;
				}

				LoadTree_r( node, childIndex, leafPtrs + leafBegin, childMinPtrs, childMaxPtrs, childLeafCount, workMem );

				leafBegin = leafEnd;
				++childIndex;
			}
		}
	}

	template <typename LT>
	static void LoadNode( spatial_tree* node, LT** leafPtrs, float * ( &leafMins )[D], float * ( &leafMaxs )[D], unsigned leafCount, void* workMem )
	{
		if( leafCount <= MAX_NODES )
			LoadLeafs( node, leafPtrs, leafMins, leafMaxs, leafCount );
		else
			LoadBranches( node, leafPtrs, leafMins, leafMaxs, leafCount, workMem );
	}

	template <typename LT>
	static void LoadTree_r( spatial_tree* parent, unsigned myIndex, LT** leafPtrs, float * ( &leafMins )[D], float * ( &leafMaxs )[D], unsigned leafCount, void* workMem )
	{
		using namespace spatial_tree_common;
		spatial_tree* me = &parent->subtree( myIndex );
		LoadNode( me, leafPtrs, leafMins, leafMaxs, leafCount, workMem );

		for( unsigned d = 0; d < D; ++d )
			AxialMinMax( me->mins[d], me->maxs[d], me->childCount, &parent->mins[d][myIndex], &parent->maxs[d][myIndex] );
	}

	static void* AllocWorkMem( unsigned leafCount, size_t *alloc_size )
	{
		using namespace spatial_tree_common;
		size_t workMemSize = ComputePartitionBoxesWorkMemSize<D>( leafCount );
		workMemSize += sizeof( unsigned ) * leafCount; // Holder for indices during build
		workMemSize += workMemSize >> 3; // Fudge factor for inter-node alignments
		workMemSize += sizeof( T* ) * leafCount; // Holds leaf pointers
		workMemSize += sizeof( float ) * D * leafCount * 2; // Holds leaf mins and maxs
		workMemSize += alignof( T* ) + alignof( float ) * D * 2; // Alignment fudge factor from work mem

		*alloc_size = workMemSize;
		void* ptr = _aligned_malloc( workMemSize + MAX_VECTOR_ALIGNMENT, MAX_VECTOR_ALIGNMENT );
		return ptr;
	}

	static void FreeWorkMem( void *ptr )
	{
		_aligned_free( ptr );
	}
};

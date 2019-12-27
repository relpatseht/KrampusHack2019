#ifndef BVH_NODE
#define BVH_NODE

#define LEAF_NODE_MASK (1u<<31u)

struct Node
{
	uvec4 childOffsets; // Top bit will be set if child is a leaf. Will be 0 if no child
	vec4 childMinX;
	vec4 childMinY;
	vec4 childMinZ;
	vec4 childMaxX;
	vec4 childMaxY;
	vec4 childMaxZ;
};



#endif //#ifdef GLSL_BVH

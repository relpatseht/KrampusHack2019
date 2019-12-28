#ifndef BVH_NODE
#define BVH_NODE

#define LEAF_NODE_ID (~0u)

struct Node
{
	uint leftIndex, rightIndex; // For leafs, leftIndex will be LEAD_NODE, right will hold entry index. 0 means no child
	float minX, minY, minZ;
	float maxX, maxY, maxZ;
};



#endif //#ifdef GLSL_BVH

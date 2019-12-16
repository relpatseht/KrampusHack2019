#version 430
#extension GL_ARB_shading_language_include : enable

#include "/hg_sdf.glsl"

const float camNear = 0.1;
const float camFar = 1000.0;

layout(location = 0) in vec3 in_worldPos;
layout(location = 1) in vec3 in_ray;

struct SceneEntry
{
	mat3 invRot;
	vec3 invPos;
	float scale;
	uint type;
};

layout(std140, binding=0) uniform SceneEntryBlock
{
	uint in_entryCount;
	SceneEntry in_entries[512];
};

layout(location = 0) uniform vec3 in_camPos;

layout (location = 0) out vec4 out_color;

vec2 SimpleScene(in const vec3 pos)
{
	const float FLT_MAX = 3.402823466e+38;
	vec2 dist = vec2(FLT_MAX, 0.0);

	for(uint i=0; i<in_entryCount; ++i)
	{
		float objScale = in_entries[i].scale;
		vec3 objPos = (in_entries[i].invRot * (pos + in_entries[i].invPos)) / objScale;
		vec2 objDist;

		switch(in_entries[i].type)
		{
			case 0:
				objDist = vec2(fSphere(objPos, 1.0)*objScale, 1.0);
			break;
			case 1:
				objDist = vec2(fBox(objPos, vec3(1.0, 1.0, 1.0))*objScale, 0.5);
			break;
			case 2:
				objDist = vec2(fCylinder(objPos, 1.0f, 1.0f)*objScale, 0.3);
			break;
			default:
				objDist = vec2(FLT_MAX, 0.0);
		}

		if(objDist.x < dist.x)
			dist = objDist;
	}

	vec2 tstDist = vec2(fSphere(pos + vec3(0.0, 0.0, -100.0f), 1.0), 0.0);

	if(tstDist.x < dist.x)
		dist = tstDist;

	return dist;
}


#define RM_SCENE_FUNC SimpleScene

#include "/raymarch.glsl"


void main()
{
	const float pixelRadius = 0.01;
	const vec3 rayDir = normalize(in_ray);
	const vec2 objDist = RayMarch(rayDir, in_camPos, pixelRadius, camFar);

	if( objDist.x == RM_INFINITY)
	{
		discard;
	}
	else
	{
		const float pixelScreenDepth = (objDist.x - camNear) / (camFar - camNear);

		gl_FragDepth = pixelScreenDepth;
		out_color = vec4(objDist.y, 1.0, 0.0, 1.0);
	}
}

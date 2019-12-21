#ifndef SCENE_MESHES
#define SCENE_MESHES

#include "/hg_sdf.glsl"
#include "/scene_defines.glsl"

void MaterialProperties(in const vec3 pos, in const float mtl, inout vec3 normal, out vec3 albedo, out float metalness, out float roughness)
{
	if(mtl < 1.0) // wood frame, [0, 1)
	{
		const vec3 bright = vec3(0.31, 0.09, 0.01)*.5;
		const vec3 dark = vec3(0.23, 0.07, 0.00)*.24;
		const float lerp = sin(pos.x*20)*noise2(pos.xy*20) + cos(pos.y*20)*noise2(pos.yx*35);
		albedo = mix(dark, bright, lerp);
		metalness = 0.01;
		roughness = 0.01;
	}
	else if(mtl < 2.0) // snow terrain, [1, 2)
	{
		albedo = vec3(0.5, 0.5, 0.5 + (2.0 - mtl)*0.2);
		metalness = 0.0;
		roughness = 0.4;
	}
	else if(mtl < 3.0) // player [2, 3)
	{
		albedo = vec3(1.0, 0.1, 0.1);
		metalness = 1.0;
		roughness = 0.1;
	}
	else if(mtl < 4.0) // helper [3, 4)
	{
		albedo = vec3(0.1, 1.0, 0.1);
		metalness = 1.0;
		roughness = 0.1;
	}
}

vec2 Mesh_Player(in const vec3 pos, float typeFrac)
{
	const float playerMtl = 2.0;
	vec3 offsetPos = pos - vec3(0.0, PLAYER_HEIGHT*0.5, 0.0); // Player drawn from feet

	return vec2(fBox(offsetPos, vec3(PLAYER_WIDTH*0.5, PLAYER_HEIGHT*0.5, PLAYER_WIDTH*0.5)), playerMtl);
}

vec2 Mesh_Helper(in const vec3 pos, float typeFrac)
{
	const float helperMtl = 3.0;

	return vec2(fSphere(pos, HELPER_RADIUS), helperMtl);
}

vec2 Mesh_SceneBounds(in const vec3 pos, float typeFrac)
{
	const float woodMtl = 0.0;
	const float boundDim = 0.1;
	float l = fBox(pos - vec3(-(BOUNDS_HALF_WIDTH + boundDim), 0, 0), vec3(boundDim, BOUNDS_HALF_HEIGHT, boundDim));
	float r = fBox(pos - vec3( (BOUNDS_HALF_WIDTH + boundDim), 0, 0), vec3(boundDim, BOUNDS_HALF_HEIGHT, boundDim));
	float t = fBox(pos - vec3( 0.0, (BOUNDS_HALF_HEIGHT + boundDim), 0), vec3(BOUNDS_HALF_WIDTH + boundDim*2, boundDim, boundDim));
	float b = fBox(pos - vec3( 0.0,-(BOUNDS_HALF_HEIGHT + boundDim), 0), vec3(BOUNDS_HALF_WIDTH + boundDim*2, boundDim, boundDim));

	return vec2(min(l, min(r, min(t, b))), woodMtl);
}

vec2 Mesh_StaticPlatforms(in vec3 pos, float typeFrac)
{
	const float woodMtl = 0.0;

	pReflect(pos, vec3(-1, 0, 0), 0.0);
	pos -= vec3(BOTTOM_LEFT_PLATFORM_X, BOTTOM_LEFT_PLATFORM_Y, 0);
	const float yCell = pModInterval1(pos.y, PLATFORM_VERTICAL_SPACE, 0, PLATFORM_COUNT-1);
	const float box = fBox(pos, vec3(BOTTOM_PLATFORM_WIDTH - yCell*PLATFORM_WIDTH_DROP, PLATFORM_DIM, PLATFORM_DIM));

	return vec2(box, woodMtl);
}

#endif //#ifndef SCENE_MESHES

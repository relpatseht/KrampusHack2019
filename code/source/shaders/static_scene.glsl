#ifndef STATIC_SCENE
#define STATIC_SCENE

#include "/hg_sdf.glsl"
#include "/noise.glsl"
#include "/scene_defines.glsl"

vec2 StaticScene_Bounds(in const vec3 pos)
{
	const float boundDim = 0.1;
	float l = fBox(pos - vec3(-(BOUNDS_HALF_WIDTH + boundDim), 0, 0), vec3(boundDim, BOUNDS_HALF_HEIGHT, boundDim));
	float r = fBox(pos - vec3( (BOUNDS_HALF_WIDTH + boundDim), 0, 0), vec3(boundDim, BOUNDS_HALF_HEIGHT, boundDim));
	float t = fBox(pos - vec3( 0.0, (BOUNDS_HALF_HEIGHT + boundDim), 0), vec3(BOUNDS_HALF_WIDTH + boundDim*2, boundDim, boundDim));
	float b = fBox(pos - vec3( 0.0,-(BOUNDS_HALF_HEIGHT + boundDim), 0), vec3(BOUNDS_HALF_WIDTH + boundDim*2, boundDim, boundDim));

	return vec2(min(l, min(r, min(t, b))), 0.0);
}

vec2 StaticScene_Ground(in vec3 pos)
{
	pos.y += 8;
	const float hNoise = noise2(pos.xz*20);
    const float height = cos(pos.x*0.25)*sin(pos.z*0.25) + 0.006*hNoise; // very regular patern + a little bit of noise

    return vec2(pos.y - height, 1.0 + hNoise*0.99);
}

vec2 StaticScene_Platforms(in vec3 pos)
{
	pReflect(pos, vec3(-1, 0, 0), 0.0);
	pos -= vec3(BOTTOM_LEFT_PLATFORM_X, BOTTOM_LEFT_PLATFORM_Y, 0);
	const float yCell = pModInterval1(pos.y, PLATFORM_VERTICAL_SPACE, 0, PLATFORM_COUNT-1);
	const float box = fBox(pos, vec3(BOTTOM_PLATFORM_WIDTH - yCell*PLATFORM_WIDTH_DROP, PLATFORM_DIM, PLATFORM_DIM));

	return vec2(box, 0.0);
}

vec2 StaticScene(in const vec3 pos)
{
	vec2 bounds = StaticScene_Bounds(pos);
	vec2 ground = StaticScene_Ground(pos);
	vec2 platforms = StaticScene_Platforms(pos);
	vec2 dist = bounds;

	if(ground.x < dist.x)
	{
		if (platforms.x < ground.x)
			dist = platforms;
		else
			dist = ground;
	}
	else if(platforms.x < dist.x)
		dist = platforms;

	return dist;
}

#endif //#ifndef STATIC_SCENE

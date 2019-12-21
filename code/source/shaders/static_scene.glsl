#ifndef STATIC_SCENE
#define STATIC_SCENE

#include "/hg_sdf.glsl"
#include "/noise.glsl"
#include "/scene_defines.glsl"


vec2 StaticScene_Ground(in vec3 pos)
{
	pos.y += 8;
	const float hNoise = noise2(pos.xz*20);
    const float height = cos(pos.x*0.25)*sin(pos.z*0.25) + 0.006*hNoise; // very regular patern + a little bit of noise

    return vec2(pos.y - height, 1.0 + hNoise*0.99);
}

vec2 StaticScene(in const vec3 pos)
{
	return StaticScene_Ground(pos);
}

#endif //#ifndef STATIC_SCENE

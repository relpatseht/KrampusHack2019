#version 430
#extension GL_ARB_shading_language_include : enable

#include "/hg_sdf.glsl"
#include "/pbr_lighting.glsl"
#include "/noise.glsl"
#include "/static_scene.glsl"
#include "/raymarch.glsl"

const float g_camNear = 0.1;
const float g_camFar = 25.0;

layout(location = 0) in vec3 in_worldPos;
layout(location = 1) in vec3 in_ray;

layout(std140, binding=0) uniform SceneEntryBlock
{
	mat4 in_entries[MAX_SCENE_ENTRIES];
};

layout(location = 0) uniform vec3 in_camPos;
layout(location = 1) uniform vec3 in_camTarget;
layout(location = 2) uniform uint in_sceneEntryCount;

layout (location = 0) out vec4 out_color;

vec2 RayMarch_SceneFunc(in vec3 pos)
{
	const float FLT_MAX = 3.402823466e+38;
	vec2 dist = vec2(FLT_MAX, 0.0);

	for(uint i=0; i<in_sceneEntryCount; ++i)
	{
		mat4 invTransform = in_entries[i];
		float type = invTransform[3][3];

		invTransform[3][3] = 1.0;

		vec3 objPos = (vec4(pos, 1.0)*invTransform).xyz;
		vec2 objDist;

		if(type < 1.0)
			objDist = vec2(fSphere(objPos, 1.0), type);
		else if(type < 2.0)
			objDist = vec2(fBox(objPos, vec3(1.0, 1.0, 1.0)), type);
		else if(type < 3.0)
				objDist = vec2(fCylinder(objPos, 1.0f, 1.0f), type);
		else if(type < 4.0)
				objDist = vec2(fCylinder(objPos, 1.0f, 1.0f), type);
		else if(type < 5.0)
				objDist = vec2(fCylinder(objPos, 1.0f, 1.0f), type);
		else if(type < 6.0)
				objDist = vec2(fCylinder(objPos, 1.0f, 1.0f), type);
		else if(type < 7.0)
				objDist = vec2(fCylinder(objPos, 1.0f, 1.0f), type);

		if(objDist.x < dist.x)
			dist = objDist;
	}

	{
		vec2 staticSceneDist = StaticScene(pos);

		if(staticSceneDist.x < dist.x)
			dist = staticSceneDist;
	}

	{
		const float noise = noise3(pos*32)*0.004 + 0.998;
		const float bottom = fSphere(pos*noise - vec3(0.0, -5.0, 0.0), 1.6) / noise;
		const float mid = fSphere(pos*noise - vec3(0.0, -2.25, 0.0), 1.25) / noise;
		const float top = fSphere(pos*noise - vec3(0.0, -0.25, 0.0), 1.0) / noise;
		const float snowman = fOpUnionRound(bottom, fOpUnionRound(mid, top, 0.45), 0.6);

		if(snowman < dist.x)
			dist = vec2(snowman, 1.0);
	}

	return dist;
}

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
}

vec3 RayDir(in const vec3 camDir)
{
	const vec2 res = vec2(1024, 768);
	const vec3 up = vec3(0, 1, 0);
	vec3 right = cross(camDir, up);
	vec2 pixel = (2.0 * (vec2(gl_FragCoord.xy) / res)) - 1.0;

	pixel.x *= res.x/res.y;

	return normalize(right*pixel.x + up*pixel.y + camDir);
}

void main()
{
	vec3 camDir = normalize(in_camTarget - in_camPos);
	vec3 rayDir = RayDir(camDir);
	vec2 objDist = RayMarch(rayDir, in_camPos, g_camFar);

	if( objDist.x == RM_INFINITY)
	{
		out_color = vec4(0.01, 0.01, 0.2, 1.0);
	}
	else
	{
		// Hit properties
		const vec3 hitPos = rayDir * objDist.x + in_camPos;
		const vec3 viewDir = normalize(in_camPos - hitPos);
		vec3 hitNormal = ComputeNormal(hitPos);

		// set depth
		float pixelScreenDepth = (objDist.x - g_camNear) / (g_camFar - g_camNear);
		gl_FragDepth = pixelScreenDepth;

		// "material properties" (todo)
		vec3 albedo = vec3(0.5, 0.0, 0.0);
		float metalness = 0.95;
		float roughness = 0.05;
		MaterialProperties(hitPos, objDist.y, hitNormal, albedo, metalness, roughness);

		// compute lighting
		const vec3 ambientLight = vec3(0.3);

		vec3 light1Color = vec3(700, 700, 1000);
		vec3 light1Pos = vec3(20, 20, 5);
		vec3 light1Dir = normalize(light1Pos - hitPos);
		float light1Dist = length(light1Pos - hitPos);
		float light1Attn = 1.0 / (light1Dist * light1Dist);
		vec3 light1BRDF = BRDF_CookTorrance(hitNormal, viewDir, light1Dir, albedo, metalness, roughness);
		float light1Shadow = RayMarch_Shadow(light1Dir, hitPos, 10.0, g_camFar);
		vec3 light1Radiance = light1Color*light1Attn*light1BRDF*light1Shadow;

		vec3 light2Color = vec3(700, 700, 1000);
		vec3 light2Pos = vec3(-20, 20, 30);
		vec3 light2Dir = normalize(light2Pos - hitPos);
		float light2Dist = length(light2Pos - hitPos);
		float light2Attn = 1.0 / (light2Dist * light2Dist);
		vec3 light2BRDF = BRDF_CookTorrance(hitNormal, viewDir, light2Dir, albedo, metalness, roughness);
		vec3 light2Radiance = light2Color*light2Attn*light2BRDF;
		float light2Shadow = RayMarch_Shadow(light2Dir, hitPos, 10.0, g_camFar);

		vec3 color = (ambientLight * albedo) + light1Radiance;// + light2Radiance;
		out_color = vec4(GammaCorrectColor(color), 1.0);
	}

	//out_color = vec4(raymarch(in_camPos, rayDir), 1.0);
}

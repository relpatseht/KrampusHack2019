#version 430
#extension GL_ARB_shading_language_include : enable

#include "/hg_sdf.glsl"
#include "/pbr_lighting.glsl"
#include "/noise.glsl"
#include "/static_scene.glsl"
#include "/raymarch.glsl"
#include "/scene_meshes.glsl"

const float g_camNear = 1.0;
const float g_camFar = 100.0;

layout(location = 0) in vec3 in_worldPos;
layout(location = 1) in vec3 in_ray;

layout(std140, binding=0) uniform SceneEntryBlock
{
	mat4 in_entries[MAX_SCENE_ENTRIES];
};

layout(location = 0) uniform vec3 in_camPos;
layout(location = 1) uniform vec3 in_camTarget;
layout(location = 2) uniform float in_camInvFov;
layout(location = 3) uniform mat3 in_camView;
layout(location = 6) uniform vec2 in_resolution;
layout(location = 7) uniform uint in_sceneEntryCount;

layout (location = 0) out vec4 out_color;

vec2 RayMarch_SceneFunc(in vec3 pos)
{
	const float FLT_MAX = 3.402823466e+38;
	vec2 dist = vec2(FLT_MAX, 0.0);

	for(uint i=0; i<in_sceneEntryCount; ++i)
	{
		mat4 invTransform = in_entries[i];
		float type = invTransform[3][3];
		float typeFrac = fract(type);

		invTransform[3][3] = 1.0;

		vec3 objPos = (invTransform*vec4(pos, 1.0)).xyz;
		vec2 objDist;

		if(type < MESH_TYPE_PLAYER + 1.0)
			objDist = Mesh_Player(objPos, typeFrac);
		else if(type < MESH_TYPE_HELPER + 1.0)
			objDist = Mesh_Helper(objPos, typeFrac);
		else if(type < MESH_TYPE_SNOW_FLAKE + 1.0)
				objDist = Mesh_Snowflake(objPos, typeFrac);
		else if(type < MESH_TYPE_SNOW_BALL + 1.0)
				objDist = Mesh_Snowball(objPos, typeFrac);
		else if(type < MESH_TYPE_SNOW_MAN + 1.0)
				objDist = vec2(fCylinder(objPos, 1.0f, 1.0f), type);
		else if(type < MESH_TYPE_STATIC_PLATFORMS + 1.0)
				objDist = Mesh_StaticPlatforms(objPos, typeFrac);
		else if(type < MESH_TYPE_WORLD_BOUNDS + 1.0)
				objDist = Mesh_SceneBounds(objPos, typeFrac);;

		if(objDist.x < dist.x)
			dist = objDist;
	}

	{
		vec2 staticSceneDist = StaticScene(pos);

		if(staticSceneDist.x < dist.x)
			dist = staticSceneDist;
	}

	{
		vec2 snowflat = Mesh_Snowball(pos - vec3(0.0, 4.0, 0.0), 0.1);
		if(snowflat.x < dist.x)
		dist = snowflat;
	}

	{
		const float noise = noise3(pos*32)*0.004 + 0.998;
		const float bottom = fSphere(pos*noise - vec3(0.0, -6.25, 0.0), 2.0) / noise;
		const float mid = fSphere(pos*noise - vec3(0.0, -3.5, 0.0), 1.5) / noise;
		const float top = fSphere(pos*noise - vec3(0.0, -1.35, 0.0), 1.1) / noise;
		const float snowman = fOpUnionRound(bottom, fOpUnionRound(mid, top, 0.2), 0.3);

		if(snowman < dist.x)
			dist = vec2(snowman, 1.0);
	}

	return dist;
}

vec3 RayDir(in const vec3 camDir)
{
	vec2 pixel = vec2(gl_FragCoord.xy) - in_resolution*0.5;
	float z = in_resolution.y * in_camInvFov;
	vec3 viewRayDir = normalize(vec3(pixel, -z));

	return in_camView * viewRayDir;
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
		vec3 light1Pos = vec3(20, 20, 15);
		vec3 light1Dir = normalize(light1Pos - hitPos);
		float light1Dist = length(light1Pos - hitPos);
		float light1Attn = 1.0 / (light1Dist * light1Dist);
		vec3 light1BRDF = BRDF_CookTorrance(hitNormal, viewDir, light1Dir, albedo, metalness, roughness);
		float light1Shadow = RayMarch_Shadow(light1Dir, hitPos, 25.0, g_camFar);
		vec3 light1Radiance = light1Color*light1Attn*light1BRDF*light1Shadow;

		vec3 light2Color = vec3(2, 10, 2);
		vec3 light2Pos = -(in_entries[1])[3].xyz;//vec3(-20, 20, 30); // Hack, helper light
		vec3 light2Dir = normalize(light2Pos - hitPos);
		float light2Dist = length(light2Pos - hitPos);
		float light2Attn = 1.0 / (light2Dist * light2Dist);
		vec3 light2BRDF = BRDF_CookTorrance(hitNormal, viewDir, light2Dir, albedo, metalness, roughness);
		//float light2Shadow = RayMarch_Shadow(light2Dir, hitPos, 10.0, g_camFar);
		vec3 light2Radiance = light2Color*light2Attn*light2BRDF;//*light2Shadow;

		vec3 color = (ambientLight * albedo) + light1Radiance + light2Radiance;
		out_color = vec4(GammaCorrectColor(color), 1.0);
	}
}

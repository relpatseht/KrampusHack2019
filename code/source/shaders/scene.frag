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
	float type;
};

layout(std140, binding=0) uniform SceneEntryBlock
{
	uint in_entryCount;
	SceneEntry in_entries[512];
};

layout(location = 0) uniform vec3 in_camPos;
layout(location = 1) uniform vec3 in_camTarget;

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

	vec2 tstDist = vec2(fSphere(pos, 2), 8);

	if(tstDist.x < dist.x)
		dist = tstDist;

	return dist;
}


#define RM_SCENE_FUNC SimpleScene

#include "/raymarch.glsl"

vec3 getRayDir(in const vec3 camDir)
{
	const vec2 res = vec2(1024, 768);
	const vec3 up = vec3(0, 1, 0);
	vec3 right = cross(camDir, up);
	vec2 pixel = (2.0 * (vec2(gl_FragCoord.xy) / res)) - 1.0;

	pixel.x *= res.x/res.y;

	return normalize(right*pixel.x + up*pixel.y + camDir);
}

vec3 ComputePhongContribution(vec3 diffuse, vec3 specular, float specPower, vec3 pos, vec3 normal, vec3 eye, vec3 lightPos, vec3 lightColor)
{
	vec3 lightDir = normalize(lightPos - pos);
	vec3 viewDir = normalize(eye - pos);
	vec3 reflectDir = normalize(reflect(-lightDir, normal));
	float lightViewDot = max(dot(lightDir, viewDir), 0);
	float reflectViewDot = dot(reflectDir, viewDir);

	if(reflectViewDot < 0)
		return lightColor * (diffuse * lightViewDot);
	else
		return lightColor * (diffuse * lightViewDot + specular * pow(reflectViewDot, specPower));
}

void main()
{
	const float pixelRadius = 0.0001;
	vec3 camDir = normalize(in_camTarget - in_camPos);
	vec3 rayDir = getRayDir(camDir);
	vec2 objDist = RayMarch(rayDir, in_camPos, pixelRadius, camFar);

	if( objDist.x == RM_INFINITY)
	{
		out_color = vec4(0.01, 0.01, 0.2, 1.0);
	}
	else
	{
		// Hit properties
		const vec3 hitPos = rayDir * objDist.x + in_camPos;
		const vec3 hitNormal = ComputeNormal(hitPos);

		// set depth
		float pixelScreenDepth = (objDist.x - camNear) / (camFar - camNear);
		gl_FragDepth = pixelScreenDepth;

		// "material properties" (todo)
		const vec3 ambient = vec3(0.2, 0.2, 0.2);
		const vec3 diffuse = vec3(0.7, 0.7, 0.1);
		const vec3 specular = vec3(1.0, 1.0, 0.8);
		const float specPower = 8;

		// compute lighting
		const vec3 ambientLight = vec3(0.1);
		vec3 light1Pos = vec3(100, 100, -100);
		vec3 light2Pos = vec3(-300, 100, 0);
		vec3 light1Color = vec3(0.3, 0.2, 0.001);
		vec3 light2Color = vec3(0.4, 0.4, 0.4);

		vec3 color = ambientLight * ambient;
		color += ComputePhongContribution(diffuse, specular, specPower, hitPos, hitNormal, in_camPos, light1Pos, light1Color);

		color += ComputePhongContribution(diffuse, specular, specPower, hitPos, hitNormal, in_camPos, light2Pos, light2Color);

		out_color = vec4(color, 1.0);
	}

	//out_color = vec4(raymarch(in_camPos, rayDir), 1.0);
}

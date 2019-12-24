#ifndef RAYMARCH_GLSL
#define RAYMARCH_GLSL

#include "hg_sdf.glsl"

#ifndef RM_STEP_MULT
#define RM_STEP_MULT 1.2
#endif //#ifndef RM_STEP_MULT

#ifndef RM_MAX_ITERATIONS
#define RM_MAX_ITERATIONS 64
#endif //#ifndef RM_MAX_ITERATIONS

#ifndef RM_NORMAL_DERIVATIVE_EPSILON
#define RM_NORMAL_DERIVATIVE_EPSILON 0.0001
#endif //#ifndef RM_NORMAL_DERIVATIVE_EPSILON

const float RM_INFINITY = (1.0/0.0);

vec2 RayMarch_SceneFunc (in vec3 pos);

// Performs enhansed sphere tracing with relaxed step size
// see http://lgdv.cs.fau.de/get/2234
//
// Expects RM_SCENE_FUNC to be defined to  a function taking the world space ray position
// and returning the distance to the closest object in the scene along with the object id.
// Distance is in X, object id in Y.
// rayDir - normalized ray direction
// rayOrigin - Start point of the ray
// pixelRadius - Radius of a pixel at rayDir*t + rayOrigin where t = 1
// farPlane - Maximum t value to consider
vec2 RayMarch(in const vec3 rayDir, in const vec3 rayOrigin, in const float farPlane, in const float pixelRadius)
{
	const float originDist = RayMarch_SceneFunc(rayOrigin).x;
	const float sceneFuncSign = sgn(originDist);
	float stepMult = RM_STEP_MULT;
	float t = originDist;
	float prevRadius = abs(originDist);
	float stepLength = 0.0;

	// Typically, sphere tracing steps along the ray by the distance to the closest object in the scene.
	// This ensures each subsequent point will be on the boundary of the sphere of the previous. In this
	// step format, we will never miss what we're aiming for.
	// To speed things up,  we take steps of the closest distance times RM_STEP_MULT.  As long as this
	// gives us an intersecting sphere, we're guarenteed not to have stepped over an obstacle. If the 
	// sphere doesn't intersect, then we need to revert back to the prior sphere's edge before proceeding
	// with the standard algorithm (no increased step size).
	for(int stepIndex=0; stepIndex < RM_MAX_ITERATIONS; ++stepIndex)
	{
		const vec3 worldPos = rayDir*t + rayOrigin;
		const vec2 sceneRet = RayMarch_SceneFunc(worldPos);
		const float signedRadius = sceneFuncSign * sceneRet.x;
		const float radius = abs(signedRadius);
		const bool overstep = stepMult > 1.0 && (radius + prevRadius) < stepLength;

		if(overstep)
		{
			// These sphere doesn't intersect the previous sphere. We may have stepped
			// over an object. Step back to the edge of the previous sphere and continue
			// the algorithm without taking large steps.
			stepLength -= stepMult * stepLength;
			stepMult = 1.0;
		}
		else
		{
			const float sphereError = radius / t;

			// If this position is within half a pixel in screen space, call it a hit.
			if(sphereError < pixelRadius)
			{
				return vec2(t, sceneRet.y);
			}

			// If we've gone past the far plane, we're done.
			if(t >= farPlane)
			{
				break;
			}

			stepLength = signedRadius * stepMult;
		}

		prevRadius = radius;
		t += stepLength;
	}

	// Never got within half a pixel of anything. We didn't hit anything.
	return vec2(RM_INFINITY, 0.0);	
}

vec2 RayMarch(in const vec3 rayDir, in const vec3 rayOrigin, in const float farPlane)
{
	return RayMarch(rayDir, rayOrigin, farPlane, 0.0001);
}

float RayMarch_Shadow(in const vec3 rayDir, in const vec3 rayOrigin, in const float hardness, in const float farPlane, in const float pixelRadius)
{
	const float originDist = RayMarch_SceneFunc(rayOrigin).x;
	const float sceneFuncSign = sgn(originDist);
	float stepMult = RM_STEP_MULT;
	float t = originDist;
	float prevRadius = abs(originDist);
	float stepLength = 0.0;
	float hit = 1.0;

	// Typically, sphere tracing steps along the ray by the distance to the closest object in the scene.
	// This ensures each subsequent point will be on the boundary of the sphere of the previous. In this
	// step format, we will never miss what we're aiming for.
	// To speed things up,  we take steps of the closest distance times RM_STEP_MULT.  As long as this
	// gives us an intersecting sphere, we're guarenteed not to have stepped over an obstacle. If the 
	// sphere doesn't intersect, then we need to revert back to the prior sphere's edge before proceeding
	// with the standard algorithm (no increased step size).
	for(int stepIndex=0; stepIndex < RM_MAX_ITERATIONS; ++stepIndex)
	{
		const vec3 worldPos = rayDir*t + rayOrigin;
		const float sceneDist = RayMarch_SceneFunc(worldPos).x;
		const float signedRadius = sceneFuncSign * sceneDist;
		const float radius = abs(signedRadius);
		const bool overstep = stepMult > 1.0 && (radius + prevRadius) < stepLength;

		if(overstep)
		{
			// These sphere doesn't intersect the previous sphere. We may have stepped
			// over an object. Step back to the edge of the previous sphere and continue
			// the algorithm without taking large steps.
			stepLength -= stepMult * stepLength;
			stepMult = 1.0;
			t += stepLength;
		}
		else
		{
			const float sphereError = radius / t;

			// Hit something, full shadow
			if(sphereError < pixelRadius)
			{
				return 0.0;
			}

			stepLength = signedRadius * stepMult;
			t += stepLength;

			hit = min(hit, hardness * sceneDist / t);

			// If we've gone past the far plane, we're done.
			if(t >= farPlane)
			{
				break;
			}
		}

		prevRadius = radius;
	}

	return clamp(hit, 0.0, 1.0);
}

float RayMarch_Shadow(in const vec3 rayDir, in const vec3 rayOrigin, in const float hardness, in const float farPlane)
{
	return RayMarch_Shadow(rayDir, rayOrigin, hardness, farPlane, 0.01);
}

// Computes a normal using the definition of a derivative with the surface position and scene function
vec3 ComputeNormal(const vec3 pos)
{
	const vec2 nd = vec2(RM_NORMAL_DERIVATIVE_EPSILON, 0.0);
	vec3 normal;

	normal.x = RayMarch_SceneFunc(pos + nd.xyy).x - RayMarch_SceneFunc(pos - nd.xyy).x;
	normal.y = RayMarch_SceneFunc(pos + nd.yxy).x - RayMarch_SceneFunc(pos - nd.yxy).x;
	normal.z = RayMarch_SceneFunc(pos + nd.yyx).x - RayMarch_SceneFunc(pos - nd.yyx).x;

	return normalize(normal);
}

#endif //#ifndef RAYMARCH_GLSL

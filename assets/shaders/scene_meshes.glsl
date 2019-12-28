#ifndef SCENE_MESHES
#define SCENE_MESHES

#include "/hg_sdf.glsl"
#include "/scene_defines.glsl"
#include "/noise.glsl"

void MaterialProperties(in const vec3 pos, in const float time, in const float mtl, inout vec3 normal, out vec3 albedo, out float metalness, out float roughness)
{
	switch(int(mtl))
	{
		case 0: // wood
		{
			const vec3 bright = vec3(0.31, 0.09, 0.01)*.5;
			const vec3 dark = vec3(0.23, 0.07, 0.00)*.24;
			const float lerp = noise(pos.xyx*20);
			albedo = mix(dark, bright, lerp)*2.6;
			metalness = 0.01;
			roughness = 0.01;
		}
		break;
		case 1: // terrain
		{
			const vec3 brightSnow = vec3(0.95, 0.8, 0.75);
			const vec3 darkSnow = vec3(0.1, 0.2, 0.5);
			float glint = noise(pos*35)+noise(vec3(mtl));
			albedo = mix(darkSnow, brightSnow, 0.8);
			if(glint > 1.5)
				albedo += darkSnow;
			metalness = 0.0;
			roughness = 0.6;
		}
		break;
		case 2: // player
		{
			albedo = vec3(1.0, 0.1, 0.1);
			metalness = 1.0;
			roughness = 0.1;
		}
		break;
		case 3: // helper
		{
			albedo = vec3(0.1, 1.0, 0.1);
			metalness = 1.0;
			roughness = 0.1;
		}
		break;
		case 4: // evergreen
		{
			const vec3 greenTree = vec3(0.04, 0.3, 0.15);
			const vec3 blueTree = vec3(0.4, 0.7, 0.86) * 0.5;
			albedo = mix(greenTree, blueTree, fract(mtl));
			metalness = 0.0;
			roughness = 1.0;
		}
		break;
		case 5: // coal eyes
		{
			albedo = vec3(0.2, 0.2, 0.2);
			metalness = 1.0;
			roughness = 0.1;
		}
		break;
		case 6: // carrot
		{
			albedo = vec3(0.8, 0.35, 0.1);
			metalness = 0.0;
			roughness = 0.4;
		}
		break;
		case 7: // snowman
		{
			const vec3 brightSnow = vec3(0.95, 0.8, 1.0);
			albedo = brightSnow;
			metalness = 0.0;
			roughness = 1.6;

			const float typeFrac = fract(mtl);
			if(typeFrac > 0.95)
			{
				albedo += albedo * (sin(time*2.0) + 1.0)*0.5 * (typeFrac - 0.95)*20.0;
			}
		}
		break;
		case 10: // snow flake
		{
			const vec3 brightSnow = vec3(0.95, 0.8, 1.0);
			const vec3 darkSnow = vec3(0.2, 0.4, 0.6);
			albedo = mix(darkSnow, brightSnow, noise(vec3(fract(mtl)*19 +94)) * 0.7 + 0.3);
			metalness = 0.0;
			roughness = 1.6;
			albedo *= 1.4;
		}
		break;
		case 11: // fireball
		{
			const vec3 Color1 = vec3(4.0, 1.0, 1.0)*2.0;
			const vec3 Color2 = vec3(1.5, 0.8, 0.2)*1.5;
			const vec3 Color3 = vec3(1.25, 0.03, 0.0)*1.5;
			const vec3 Color4 = vec3(0.05, 0.02, 0.02);
			float noise = fract(mtl);
			float c1 = saturate(noise*5.0 + 0.5);
			float c2 = saturate(noise*5.0);
			float c3 = saturate(noise*3.4 - 0.5);
			vec3 a = mix(Color1,Color2, c1);
			vec3 b = mix(a,     Color3, c2);

			albedo = mix(b,     Color4, c3) * 1.2;
			metalness = 1.0;
			roughness = 0.04;
		}
		break;
	}
}

vec2 Mesh_Player(in const vec3 pos, float typeFrac)
{
	const float playerMtl = 2.0;
	const vec3 bottom = vec3(0, PLAYER_WIDTH*0.5, 0.0);
	const vec3 top = vec3(0.0, PLAYER_HEIGHT - PLAYER_WIDTH*0.5, 0.0);

	return vec2(fCapsule(pos, bottom, top, PLAYER_WIDTH*0.5), playerMtl);
}

vec2 Mesh_Helper(in const vec3 pos, float typeFrac)
{
	const float helperMtl = 3.0;

	return vec2(fSphere(pos, HELPER_RADIUS), helperMtl);
}

vec2 Mesh_Snowflake(in vec3 pos, float typeFrac)
{
	const float hitFlake = fSphere(pos, SNOWFLAKE_RADIUS);

	if(hitFlake <= 0.1)
	{
		const float flakeMtl = 10.0;
		pos /= SNOWFLAKE_RADIUS;

		float hexSubtraction;

		{ // snowflake subtractions
			float radialBar;
			float leafBox;
			float centerHex;

			{ // bar
				vec3 barPos = pos;
				float barIter = floor(typeFrac * 3.0); // typeFrac is [0, 1), we want [0, 1, 2]
				float barWidth = noise(vec3(typeFrac*30 + 12)) * 0.1 + 0.05;

				pModPolar(barPos.yx, barIter*6.0);
				barPos -= vec3(0, 0.8, 0);
				radialBar = fBox(barPos, vec3(barWidth, 0.4, 0.3));
			}

			{ // box
				vec3 boxPos = pos;
				float boxHeight = noise(vec3(typeFrac*25 + 5)) * 0.13 + 0.1;
				float boxWidth = noise(vec3(typeFrac*29 + 15)) * 0.1 + 0.06;

				pR(boxPos.xy, PI*0.5);
				pModPolar(boxPos.yx, 6.0);
				boxPos -= vec3(0.0, 0.8, 0);
				leafBox = fBox(boxPos, vec3(boxHeight, boxWidth, 0.3));
			}

			{ // hex
				vec3 hexPos = pos;
				float hexIter = floor(noise(vec3(typeFrac*13 + 76)) * 2.0) * 0.5; // typeFrac is [0, 1), we want [0, 0.5]
				float hexRadius = noise(vec3(typeFrac*32 + 17)) * 0.3 + 0.1;

				pR(hexPos.yz, PI*0.5);
				pR(hexPos.xz, PI*hexIter);
				centerHex = fHexagonCircumcircle(hexPos, vec2(hexRadius, 0.2));
			}

			hexSubtraction = min(centerHex, min(leafBox, radialBar));
		}

		float flake;
		if(typeFrac > 0.4 && noise(vec3(typeFrac*67 -21)) > 0.5) // >0.3 since those have no radial bars and end up looking bad
			flake = hexSubtraction;
		else
		{
			vec3 baseHexPos = pos;
			pR(baseHexPos.yz, PI*0.5);
			const float baseHex = fHexagonCircumcircle(baseHexPos, vec2(1.0, 0.1));
			const float chamferRadius = noise(vec3(typeFrac*3 + 7)) * 0.1 + 0.05;
			flake = fOpDifferenceChamfer(baseHex, hexSubtraction, chamferRadius) * SNOWFLAKE_RADIUS;
		}

		flake *= SNOWFLAKE_RADIUS;
		return vec2(flake, flakeMtl + typeFrac);
	}

	return vec2(hitFlake, 0.0);
}

vec2 Mesh_Snowball(in vec3 pos, float typeFrac)
{
	const float hitBall = fSphere(pos, SNOWBALL_RADIUS);

	if(hitBall <= 0.5) // snowball isn't valid dist. need to overestimate
	{
		const float snowMtl = -1.0;
		float dist = fBlob(pos / SNOWBALL_RADIUS) * SNOWBALL_RADIUS;

		return vec2(dist, snowMtl);
	}

	return vec2(hitBall, 0.0);
}

vec2 Mesh_Snowman(in vec3 pos, float typeFrac)
{
	const float snowmanMtl = 7.0;
	const float noise = noise(pos*32)*0.001 + 0.999;
	const float bottom = fSphere(pos*noise - vec3(SNOWMAN_X, SNOWMAN_BOT_Y, SNOWMAN_Z), SNOWMAN_BOT_RADIUS) / noise;
	const float mid =    fSphere(pos*noise - vec3(SNOWMAN_X, SNOWMAN_MID_Y, SNOWMAN_Z), SNOWMAN_MID_RADIUS) / noise;
	const float top =    fSphere(pos*noise - vec3(SNOWMAN_X, SNOWMAN_TOP_Y, SNOWMAN_Z), SNOWMAN_TOP_RADIUS) / noise;
	const float snowman = fOpUnionRound(bottom, fOpUnionRound(mid, top, 0.2), 0.3);
	vec2 dist = vec2(snowman, snowmanMtl + typeFrac);

	if(typeFrac > 0.99)
	{
		const float coalMtl = 5.0;
		const float carrotMtl = 6.0;
		const vec3 headPos = pos - vec3(SNOWMAN_X, SNOWMAN_TOP_Y, SNOWMAN_Z);

		vec3 eyePos = headPos - vec3(0.0, 0.3, 0.9);
		pMirror(eyePos.x, 0.0);
		const float eyes = fSphere(eyePos - vec3(0.3, 0, 0), 0.2);

		if(eyes < dist.x)
			dist = vec2(eyes, coalMtl);

		vec3 carrotPos = headPos - vec3(0.0, 0.0, SNOWMAN_TOP_RADIUS);
		pR(carrotPos.yz, PI*0.5);
		const float nose = fCone(carrotPos, 0.15, 0.4);

		if(nose < dist.x)
			dist = vec2(nose, carrotMtl);
	}
	else
	{
		const float snowmanHeight = (SNOWMAN_TOP_Y + SNOWMAN_TOP_RADIUS) - (SNOWMAN_BOT_Y - SNOWMAN_BOT_RADIUS);
		const float visibleBox = fBoxCheap(pos - vec3(SNOWMAN_X, SNOWMAN_BOT_Y - SNOWMAN_BOT_RADIUS, SNOWMAN_Z), vec3(SNOWMAN_BOT_RADIUS*1.2, snowmanHeight*typeFrac, SNOWMAN_BOT_RADIUS*1.2));

		dist.x = max(snowman, visibleBox);
	}

	return dist;
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

// fireball from https://www.shadertoy.com/view/MtXSzS
vec2 Mesh_Fireball(in vec3 pos, float typeFrac, float time)
{
	const float fireMtl = 11.0;
	float hitBall = fSphere(pos, FIREBALL_RADIUS);

	if(hitBall <= 1.0)
	{
		const float NoiseAmplitude = 0.06;
		const float NoiseFrequency = 4.0;
		const vec3 Animation = vec3(0.0, -3.0, 0.5);

		float noise = Turbulence(pos * NoiseFrequency + Animation*time*1.8, 0.1, 1.5, 0.03) * NoiseAmplitude;
		noise = saturate(abs(noise));

		return vec2(hitBall - noise, fireMtl + noise - 0.0001);
	}

	return vec2(hitBall, fireMtl);
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

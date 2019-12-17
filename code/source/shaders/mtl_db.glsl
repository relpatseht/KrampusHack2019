#ifndef MTL_DB
#define MTL_DB

struct BRDF
{
	vec3 F0;
};

const BRDF BRDF_WATER = BRDF( vec3(0.02, 0.02, 0.02) );

struct Material
{
	vec3 albedo;
	float metalness;
	float roughness;
	float ambientOcclusion;
}

#endif //#ifndef MTL_DB

#ifndef PBR_LIGHTING
#define PBR_LIGHTING

#ifndef PI
# define PI 3.14159265358979
#endif //#ifndef PI


#ifndef INV_PI
# define INV_PI 0.3183098861838
#endif //#ifndef PI

// Trowbridge-Reitz GGX normal distribution function
// halfView is the view-light half vector
// roughness should be between 0 and 1, 0 is perfectly smooth, 1 is perfectly matte
float Normal_DistributionGGX(in const float nDotH, in const vec3 halfView, in const float roughness)
{
	// Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"
    float oneMinusNDotHSquared = 1.0 - nDotH * nDotH;
    float a = nDotH * roughness;
    float k = roughness / (oneMinusNDotHSquared + a * a);
    float d = k * k * INV_PI;
    return d;
}

// Roughness to k conversion for Schlick GGX with direct lighting
// roughness should be between 0 and 1, 0 is perfectly smooth, 1 is perfectly matte
float Geometry_RoughnessToKDirect(in const float roughness)
{
	const float rP1 = roughness + 1.0;

	return (rP1 * rP1) * 0.125;
}

// Roughness to k conversion for Schlick GGX with image based lighting
// roughness should be between 0 and 1, 0 is perfectly smooth, 1 is perfectly matte
float Geometry_RoughnessToKIBL(in const float roughness)
{
	return (roughness * roughness) * 0.5;
}

// Schlick GGX geometry distribution function
// viewAngle is cosine of the view angle (normal dot view, or normal dot light)
// K is computed from roughness with either RoughnessToKDirect or RoughnessToKIBL
float Geometry_SchlickGGX(in const float viewAngle, in const float k)
{
	return viewAngle / ((viewAngle * (1.0 - k)) + k);
}

// Smith's geometry distribution method, using SchlickGGX for account for both obstruction and shadowing
// K is computed from roughness with either RoughnessToKDirect or RoughnessToKIBL
float Geometry_Smith(in const float nDotV, in const float nDotL, in const float k)
{
	const float ggx1 = Geometry_SchlickGGX(nDotV, k);
	const float ggx2 = Geometry_SchlickGGX(nDotL, k);

	return ggx1 * ggx2;
}

// metallness is a [0, 1] value for how "metal" a surface is (metal should be binary, but to approximate dust, etc)
// surfaceColor is the tint applied by metal surfaces
vec3 Fresnel_ApproximateF0(in const float metalness, in const vec3 surfaceColor)
{
	// 0.04 is a rough approximation of a dielectric fresnel F0...
	return mix(vec3(0.04), surfaceColor, metalness);
}

// cosTheta is the cosine of the angle at which the vector hits the surface
// F0 is the base reflectivity of the surface when view from a 0 degree angle (looking directly at it)
vec3 Fresnel_Schlick(in const float cosTheta, in const vec3 F0)
{
	return F0 + ((1.0 - F0) * pow(1.0 - cosTheta, 5.0));
}

vec3 BRDF_CookTorrance(in const vec3 normal, in const vec3 view, in const vec3 lightDir, in const vec3 surfAlbedo, in const float surfMetalness, in const float surfRoughness)
{
	const vec3 halfView = normalize(view + lightDir);
	const float nDotH = max(dot(normal, halfView), 0.0);
	const float nDotV = max(dot(normal, view), 0.0);
	const float nDotL = max(dot(normal, lightDir), 0.0);
	const float hDotV = max(dot(halfView, view), 0.0);
	const float N = Normal_DistributionGGX(nDotH, halfView, surfRoughness);
	const float G = Geometry_Smith(nDotV, nDotL, Geometry_RoughnessToKDirect(surfRoughness));
	const vec3 F = Fresnel_Schlick(hDotV, Fresnel_ApproximateF0(surfMetalness, surfAlbedo));
	const vec3 kS = F;
	const vec3 kD = (vec3(1.0) - kS) * (1.0 - surfMetalness);
	const vec3 numerator = N * G * F;
	const float denominator = 4.0 * nDotV * nDotL;
	const vec3 specular = numerator / max(denominator, 0.00001);
	const vec3 lightRadiance = (kD * surfAlbedo * INV_PI) + specular * nDotL;

	return lightRadiance;
}

vec3 Tonemap_ACES(const vec3 x) {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

vec3 GammaCorrectColor(in vec3 color)
{
	return pow(color, vec3(1.0/2.2));
}

#endif //#ifdef PBR_LIGHTING

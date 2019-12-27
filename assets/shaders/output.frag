#version 430
#extension GL_ARB_shading_language_include : enable

#include "/pbr_lighting.glsl"
#include "/scene_defines.glsl"

layout(binding = 0) uniform sampler2D in_textures[BLOOM_BLUR_PASSES+1];
layout(location = 11) uniform vec2 in_resolution;

layout (location = 0) out vec4 out_color;

void main()
{
	const vec2 texCoord = gl_FragCoord.xy / in_resolution;
	vec3 color = vec3(0);

	for(uint p=0; p<BLOOM_BLUR_PASSES + 1; ++p)
		color += texture2D(in_textures[p], texCoord).xyz;

	color = Tonemap_ACES(color);
	out_color = vec4(color, 1.0);//vec4(GammaCorrectColor(color), 1.0);
}

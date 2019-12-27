#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "/scene_defines.glsl"

layout(binding = 0) uniform sampler2D in_textures;
layout(location = 10) uniform bool in_horizontal;
layout(location = 11) uniform vec2 in_resolution;

layout (location = 0) out vec4 out_color;

void main()
{
	const vec2 texCoord = gl_FragCoord.xy / in_resolution;
	const vec2 offset = in_horizontal ? vec2(1.2 / in_resolution.x, 0.0) : vec2(0.0, 1.2/in_resolution.y);
	vec3 color = vec3(0);

	color += texture(in_textures, texCoord - offset).xyz * 5.0/16.0;
	color += texture(in_textures, texCoord).xyz * 6.0/16.0;
	color += texture(in_textures, texCoord + offset).xyz * 5.0/16.0;

	out_color = vec4(color, 1.0);
}

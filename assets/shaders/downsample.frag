#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout(binding = 0) uniform sampler2D in_texture;
layout(location = 1) uniform vec2 in_resolution;

layout (location = 0) out vec4 out_color;

void main()
{
	const vec2 texCoord = gl_FragCoord.xy / in_resolution;
	const vec3 color = texture(in_texture, texCoord).xyz;

	out_color = vec4(color, 1.0);
}

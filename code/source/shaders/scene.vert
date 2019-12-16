#version 430
#extension GL_ARB_shading_language_include : enable

layout(location = 0) out vec3 out_worldPos;
layout(location = 1) out vec3 out_ray;

layout(location = 0) uniform vec3 in_camPos;
layout(location = 1) uniform vec3 in_camTarget;

// Use with a triangle strip of 4 vertices
void main()
{
	int vertId = int(gl_VertexID);
	int x = ((vertId % 2) * 2) - 1; // -1, 1, -1, 1
	int y = ((vertId / 2) * 2) - 1; // -1, -1, 1, 1

	const vec3 camUp = vec3(0.0, 1.0, 0.0);
	vec3 camDir = in_camTarget - in_camPos;
	vec3 camRight = normalize(cross(camUp, camDir));
	vec3 up = cross(camDir, camRight);

	mat4 view = mat4(camRight.x, up.x, camDir.x, 0.0,
	                 camRight.y, up.y, camDir.y, 0.0,
				     camRight.z, up.z, camDir.z, 0.0,
					 -dot(camRight, in_camPos), -dot(up, in_camPos), -dot(camDir, in_camPos), 1.0);

	vec4 modelPos = vec4(x, y, 0.0, 1.0);

	out_worldPos = (view * modelPos).xyz;
	// No need to normalize this, since we'll need to normalize in the fragment
	// shader anyway (linear interpolation doesn't preserve length)
	out_ray = out_worldPos - in_camPos;

	gl_Position = modelPos;
}

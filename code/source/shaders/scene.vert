#version 430
#extension GL_ARB_shading_language_include : enable

// Use with a triangle strip of 4 vertices
void main()
{
	int vertId = int(gl_VertexID);
	int x = ((vertId % 2) * 2) - 1; // -1, 1, -1, 1
	int y = ((vertId / 2) * 2) - 1; // -1, -1, 1, 1

	gl_Position = vec4(x, y, 0.0, 1.0);
}

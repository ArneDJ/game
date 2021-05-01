#version 430 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec2 uv;

out vec2 texcoords;

uniform mat4 PROJECT, VIEW;

void main(void)
{
	texcoords = uv;

	// spherical billboards
	mat4 T = mat4(1.0);
	T[3].xyz = color;
	mat4 MV = VIEW * T;

	// Column 0:
	MV[0][0] = 1;
	MV[0][1] = 0;
	MV[0][2] = 0;
	// Column 1:
	MV[1][0] = 0;
	MV[1][1] = 1;
	MV[1][2] = 0;
	// Column 2:
	MV[2][0] = 0;
	MV[2][1] = 0;
	MV[2][2] = 1;

	gl_Position = PROJECT * MV * vec4(position, 1.0);
}  

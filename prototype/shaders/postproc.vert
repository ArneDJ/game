#version 430 core

layout (location = 0) in vec2 vposition;
layout (location = 1) in vec2 vtexcoords;

out vec2 texcoords;

void main(void)
{
	gl_Position = vec4(vposition.x, vposition.y, 0.0, 1.0); 
	texcoords = vtexcoords;
} 

#version 430 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec2 uv;

out vec2 texcoords;

uniform mat4 PROJECT;

void main(void)
{
	texcoords = uv;

	gl_Position = PROJECT * vec4(position, 1.0);
}  

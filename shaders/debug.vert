#version 430 core

layout(location = 0) in vec3 vposition;
layout(location = 2) in vec2 vtexcoords;

out vec2 texcoords;

uniform mat4 VP;

void main(void)
{
	texcoords = vtexcoords;
	gl_Position = VP * vec4(vposition, 1.0);
}


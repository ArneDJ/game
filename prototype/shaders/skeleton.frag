#version 430 core

in vec3 normal;
in vec2 texcoords;

out vec4 fcolor;

uniform vec3 COLOR;

void main(void)
{
	//vec3 color = normal + vec3(1.0, 1.0, 1.0);
	fcolor = vec4(COLOR, 1.0);
}

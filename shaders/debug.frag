#version 430 core

out vec4 fcolor;

uniform vec3 COLOR;

void main(void)
{
	fcolor = vec4(COLOR, 1.0);
}

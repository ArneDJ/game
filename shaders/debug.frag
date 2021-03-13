#version 430 core

out vec4 fcolor;

uniform vec3 COLOR;

void main(void)
{
	//vec4 color = vec4(COLOR, 1.0);
	fcolor = vec4(0.0, 1.0, 0.0, 1.0);
}

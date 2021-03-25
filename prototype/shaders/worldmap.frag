#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;

void main(void)
{
	vec4 color = vec4(1.0, 0.0, 0.0, 1.0);
	fcolor = color;
}

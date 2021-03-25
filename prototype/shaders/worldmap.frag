#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;

void main(void)
{
	vec4 color = texture(DISPLACEMENT, fragment.texcoord);
	fcolor = vec4(vec3(color.r), 1.0);
}

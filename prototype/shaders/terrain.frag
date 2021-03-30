#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;

void main(void)
{
	float height = texture(DISPLACEMENT, fragment.texcoord).r;
	fcolor = vec4(vec3(height), 1.0);
}

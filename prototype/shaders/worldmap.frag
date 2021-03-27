#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;
layout(binding = 1) uniform sampler2D RAINMAP;
layout(binding = 2) uniform sampler2D RELIEFMAP;
layout(binding = 3) uniform sampler2D BIOMEMAP;

void main(void)
{
	float height = texture(DISPLACEMENT, fragment.texcoord).r;
	vec4 color = texture(RELIEFMAP, fragment.texcoord);
	vec4 biomecolor = texture(BIOMEMAP, fragment.texcoord);
	fcolor = vec4(mix(vec3(height), vec3(color.r), 0.1), 1.0);
}

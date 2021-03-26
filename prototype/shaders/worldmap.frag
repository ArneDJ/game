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
	vec4 color = texture(BIOMEMAP, fragment.texcoord);
	fcolor = vec4(color.rgb, 1.0);
}

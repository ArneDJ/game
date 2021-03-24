#version 430 core

in vec2 texcoords;
in vec3 normal;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D BASEMAP;
layout(binding = 1) uniform sampler2D COLORMAP;

uniform vec3 COLOR;

void main(void)
{
	vec4 color = texture(COLORMAP, texcoords);
	fcolor = vec4(vec3(color.r), 1.0);
}
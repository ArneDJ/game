#version 430 core

in vec2 texcoords;
in vec3 normal;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D BASEMAP;

uniform vec3 COLOR;

void main(void)
{
	vec4 color = texture(BASEMAP, texcoords);
	fcolor = color;
}

#version 430 core

in vec2 texcoords;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D BASEMAP;

uniform vec3 COLOR;

void main(void)
{
	//fcolor = vec4(COLOR, 1.0);
	fcolor = texture(BASEMAP, texcoords);
}

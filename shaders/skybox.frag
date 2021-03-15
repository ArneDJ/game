#version 430 core

out vec4 fcolor;

in vec3 texcoords;

uniform vec3 COLOR_TOP;
uniform vec3 COLOR_BOTTOM;

void main(void)
{
	vec3 position = normalize(texcoords);
	//float dist = 0.5 * (position.y+1.0);
	vec3 color = mix(COLOR_BOTTOM, COLOR_TOP, position.y);

	fcolor = vec4(color, 1.0);
}

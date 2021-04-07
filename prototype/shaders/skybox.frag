#version 430 core

in vec3 texcoords;

out vec4 fcolor;

uniform vec3 COLOR_TOP;
uniform vec3 COLOR_BOTTOM;
uniform vec3 SUN_POS;

vec3 sun_color(vec3 pos, float expo)
{
	float sun = clamp(dot(SUN_POS, pos), 0.0, 1.0);
	vec3 color = 0.8 * vec3(1.0, 0.6, 0.1) * pow(sun, expo);

	return color;
}

void main(void)
{
	vec3 position = normalize(texcoords);
	//float dist = 0.5 * (position.y+1.0);
	vec3 color = mix(COLOR_BOTTOM, COLOR_TOP, position.y);
	//vec3 color = mix(COLOR_BOTTOM, COLOR_TOP, clamp(1 - exp(8.5-17.*clamp(position.y*0.5 + 0.5,0.0,1.0)),0.0,1.0));

	// add sun to the sky
	color += sun_color(position, 500.0);

	fcolor = vec4(color, 1.0);
}

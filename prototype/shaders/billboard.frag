#version 430 core

in vec2 texcoords;
in vec3 position;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D BASEMAP;

// atmosphere
uniform vec3 CAM_POS;
uniform vec3 SUN_POS;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;

vec3 fog(vec3 color, float dist)
{
	float amount = 1.0 - exp(-dist * FOG_FACTOR);
	return mix(color, FOG_COLOR, amount);
}

void main(void)
{
	vec4 color = texture(BASEMAP, texcoords);

	if (color.a < 0.1) { discard; }
	color.a = 1.0;

	fcolor = vec4(fog(color.rgb, distance(CAM_POS, position)), 1.0);
}

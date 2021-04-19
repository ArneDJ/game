#version 430 core

in vec2 texcoords;
in vec3 position;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D BASEMAP;

uniform vec3 COLOR;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;
uniform vec3 CAM_POS;

vec3 fog(vec3 color, float dist)
{
	float amount = 1.0 - exp(-dist * FOG_FACTOR);
	return mix(color, FOG_COLOR, amount );
}

void main(void)
{
	vec4 color = texture(BASEMAP, texcoords);
	if (color.a < 0.1) { discard; }
	float dist = distance(CAM_POS, position);
	color.a = 0.6;
	color.rgb *= COLOR;
	//fcolor = vec4(1.0, 0.0, 1.0, 1.0);
	
	fcolor = vec4(fog(color.rgb, dist), 1.0);
}

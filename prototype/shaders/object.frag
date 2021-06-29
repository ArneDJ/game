#version 430 core

in vec2 texcoords;
in vec3 normal;
in vec3 position;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D BASEMAP;

uniform vec3 SUN_POS;
uniform vec3 CAM_POS;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;

vec3 fog(vec3 color, float dist)
{
	float amount = 1.0 - exp(-dist * FOG_FACTOR);
	return mix(color, FOG_COLOR, amount );
}

void main(void)
{
	vec4 color = texture(BASEMAP, texcoords);

	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	float diffuse = max(0.0, dot(normal, SUN_POS));
	vec3 scatteredlight = lightcolor * diffuse;
	color.rgb = mix(min(color.rgb * scatteredlight, vec3(1.0)), color.rgb, 0.5);

	color.rgb = fog(color.rgb, distance(CAM_POS, position));

	fcolor = color;

	float gamma = 1.2;
	fcolor.rgb = pow(fcolor.rgb, vec3(1.0/gamma));
}

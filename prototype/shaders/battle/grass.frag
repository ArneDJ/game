#version 430 core

in vec2 texcoords;
in vec3 position;
in float diffuse;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D BASEMAP;

uniform vec3 COLOR;
uniform vec3 AMBIANCE_COLOR;
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
	color.rgb *= COLOR;
	
	float blending = 1.0 / (0.01*dist);
	color.a *= blending*blending;
	if (color.a < 0.1) { discard; }
	if (color.a > 0.2) { color.a = 1.0; }

	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	vec3 scatteredlight = lightcolor * diffuse;
	color.rgb = mix(min(color.rgb * scatteredlight, vec3(1.0)), color.rgb, 0.5);
	
	fcolor = vec4(fog(color.rgb, dist), color.a);

	fcolor.rgb *= AMBIANCE_COLOR;

	//fcolor = vec4(1.0, 0.0, 1.0, 1.0);
}

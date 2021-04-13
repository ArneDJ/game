#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;
layout(binding = 1) uniform sampler2D NORMALMAP;
layout(binding = 2) uniform sampler2D RAINMAP;
layout(binding = 3) uniform sampler2D MASKMAP;
// material textures
layout(binding = 4) uniform sampler2D STONEMAP;
layout(binding = 5) uniform sampler2D SANDMAP;
layout(binding = 6) uniform sampler2D SNOWMAP;

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
	float height = texture(DISPLACEMENT, fragment.texcoord).r;

	vec3 normal = texture(NORMALMAP, fragment.texcoord).rgb;
	normal = (normal * 2.0) - 1.0;
	normal = normalize(normal);

	float slope = 1.0 - normal.y;
	slope = smoothstep(0.5, 0.7, slope);
	
	vec3 stone = texture(STONEMAP, 100.0 * fragment.texcoord).rgb;
	vec3 sand = texture(SANDMAP, 100.0 * fragment.texcoord).rgb;
	vec3 snow = texture(SNOWMAP, 100.0 * fragment.texcoord).rgb;
	
	float snowlevel = texture(MASKMAP, fragment.texcoord).r;
	
	vec3 color = mix(sand, snow, snowlevel);
	color = mix(color, stone, slope);

	// terrain lighting
	const vec3 lightdirection = vec3(0.5, 0.93, 0.1);
	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	float diffuse = max(0.0, dot(normal, lightdirection));
	vec3 scatteredlight = lightcolor * diffuse;
	color = mix(min(color * scatteredlight, vec3(1.0)), color, 0.5);

	color = fog(color, distance(CAM_POS, fragment.position));

	fcolor = vec4(color, 1.0);
}

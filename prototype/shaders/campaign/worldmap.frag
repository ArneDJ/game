#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

out vec4 fcolor;

uniform sampler2D DISPLACEMENT;
uniform sampler2D NORMALMAP;
uniform sampler2D RAINMAP;
uniform sampler2D TEMPERATUREMAP;
uniform sampler2D MASKMAP;
uniform sampler2D FACTIONSMAP;
// material textures
uniform sampler2D STONEMAP;
uniform sampler2D SANDMAP;
uniform sampler2D SNOWMAP;
uniform sampler2D GRASSMAP;
//uniform sampler2D FARMMAP;

uniform vec3 CAM_POS;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;

// color palette
uniform vec3 GRASS_DRY;
uniform vec3 GRASS_LUSH;
uniform vec3 ROCK_BASE_MIN;
uniform vec3 ROCK_BASE_MAX;
uniform vec3 ROCK_DESERT_MIN;
uniform vec3 ROCK_DESERT_MAX;

uniform float FACTION_FACTOR = 1.0;

vec3 fog(vec3 color, float dist)
{
	float amount = 1.0 - exp(-dist * FOG_FACTOR);
	return mix(color, FOG_COLOR, amount );
}

float random(in vec2 st)
{
	return fract(sin(dot(st.xy, vec2(12.9898,78.233)))* 43758.5453123);
}

// Based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise(in vec2 st)
{
	vec2 i = floor(st);
	vec2 f = fract(st);

	// Four corners in 2D of a tile
	float a = random(i);
	float b = random(i + vec2(1.0, 0.0));
	float c = random(i + vec2(0.0, 1.0));
	float d = random(i + vec2(1.0, 1.0));

	vec2 u = f * f * (3.0 - 2.0 * f);

	return mix(a, b, u.x) + (c - a)* u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(in vec2 st)
{
	// Initial values
	float value = 0.0;
	float amplitude = .5;
	// Loop of octaves
	for (int i = 0; i < 5; i++) {
		value += amplitude * noise(st);
		st *= 2.;
		amplitude *= .5;
	}
	return value;
}

float shiftfbm(in vec2 _st)
{
	float v = 0.0;
	float a = 0.5;
	vec2 shift = vec2(100.0);
	// Rotate to reduce axial bias
	mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.50));
	for (int i = 0; i < 5; ++i) {
		v += a * noise(_st);
		_st = rot * _st * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}

float warpfbm(vec2 st)
{
	vec2 q = vec2(0.);
	q.x = shiftfbm(st);
	q.y = shiftfbm(st + vec2(1.0));
	vec2 r = vec2(0.);
	r.x = shiftfbm( st + 20.0*q + vec2(1.7,9.2)+ 0.15);
	r.y = shiftfbm( st + 20.0*q + vec2(8.3,2.8)+ 0.126);

	return shiftfbm(st+r);
}

void main(void)
{
	float height = texture(DISPLACEMENT, fragment.texcoord).r;

	vec3 normal = texture(NORMALMAP, fragment.texcoord).rgb;

	float strata = warpfbm(0.01 * fragment.position.xz);

	float slope = 1.0 - normal.y;
	slope = smoothstep(0.7, 0.8, slope);
	
	vec3 factions = texture(FACTIONSMAP, fragment.texcoord).rgb;

	vec3 stone = texture(STONEMAP, 100.0 * fragment.texcoord).rgb;
	vec3 sand = texture(SANDMAP, 200.0 * fragment.texcoord).rgb;
	vec3 snow = texture(SNOWMAP, 100.0 * fragment.texcoord).rgb;
	vec3 grass = texture(GRASSMAP, 200.0 * fragment.texcoord).rgb;
	//vec3 farms = texture(FARMMAP, 200.0 * fragment.texcoord).rgb;
	
	vec4 mask = texture(MASKMAP, fragment.texcoord);
	float snowlevel = mask.r;
	float grasslevel = mask.g;
	float sandlevel = mask.b;
	float aridlevel = mask.a;

	float rainlevel = texture(RAINMAP, fragment.texcoord).r;
	float temperature = texture(TEMPERATUREMAP, fragment.texcoord).r;
	float exposure = smoothstep(0.6, 0.85, temperature);
	
	vec3 grassness = mix(GRASS_DRY*vec3(1.8), GRASS_LUSH, rainlevel);
	grass *= grassness;
	
	sand *= ROCK_DESERT_MIN * vec3(1.2);

	stone = mix(ROCK_BASE_MIN*stone, ROCK_BASE_MAX*stone, temperature);

	stone = mix(stone, stone * ROCK_DESERT_MIN * vec3(1.3), aridlevel);

	vec3 color = stone;
	// noisy transition
	float transition_noise = warpfbm(0.1*fragment.position.xz);
	transition_noise = smoothstep(0.25, 0.75, transition_noise);

	sandlevel = mix(transition_noise, sandlevel, 2.f * distance(sandlevel, 0.5f));
	color = mix(color, sand, sandlevel);

	grasslevel = mix(transition_noise, grasslevel, 2.f * distance(grasslevel, 0.5f));
	color = mix(color, grass, grasslevel);

	snowlevel = mix(transition_noise, snowlevel, 2.f * distance(snowlevel, 0.5f));
	color = mix(color, snow, snowlevel);
	color = mix(color, stone, slope);

	color = mix(color, strata*color, 0.5);

	color = mix(color, factions, 0.8*FACTION_FACTOR);

	// terrain lighting
	const vec3 lightdirection = vec3(0.5, 0.93, 0.1);
	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	float diffuse = max(0.0, dot(normal, lightdirection));
	vec3 scatteredlight = lightcolor * diffuse;
	color = mix(min(color * scatteredlight, vec3(1.0)), color, 0.5);

	color = fog(color, distance(CAM_POS, fragment.position));

	fcolor = vec4(color, 1.0);
	//fcolor = vec4(stone, 1.0);
	//fcolor = vec4(vec3(volcanism), 1.0);

	float gamma = 1.2;
	fcolor.rgb = pow(fcolor.rgb, vec3(1.0/gamma));
}

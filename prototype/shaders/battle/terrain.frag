#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	float zclipspace;
} fragment;

out vec4 fcolor;

uniform sampler2D DISPLACEMENT;
uniform sampler2D NORMALMAP;
uniform sampler2D SITEMASKS;
uniform sampler2D STONEMAP;
uniform sampler2D SANDMAP;
uniform sampler2D GRASSMAP;
uniform sampler2D GRAVELMAP;
uniform sampler2D DETAILMAP;

uniform vec3 MAP_SCALE;
uniform vec2 SITE_OFFSET;
uniform vec2 SITE_SCALE;
// atmosphere
uniform vec3 CAM_POS;
uniform vec3 SUN_POS;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;

uniform vec3 GRASS_COLOR;

vec3 fog(vec3 color, float dist)
{
	float amount = 1.0 - exp(-dist * FOG_FACTOR);
	return mix(color, FOG_COLOR, amount );
}

/*
float filterPCF(vec4 sc)
{
	ivec2 size = textureSize(SHADOWMAP, 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(size.x);
	float dy = scale * 1.0 / float(size.y);

	float shadow = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			vec4 coords = sc;
			coords.x += dx*x;
			coords.y += dy*y;
			shadow += texture(SHADOWMAP, coords);
			count++;
		}
	}

	return shadow / count;
}

float shadow_coef(void)
{
	float shadow = 1.0;

	int cascade = 0;
	if (fragment.zclipspace < SPLIT.x) {
		cascade = 0;
	} else if (fragment.zclipspace < SPLIT.y) {
		cascade = 1;
	} else if (fragment.zclipspace < SPLIT.z) {
		cascade = 2;
	} else if (fragment.zclipspace < SPLIT.w) {
		cascade = 3;
	} else {
		return 1.0;
	}

	vec4 coords = SHADOWSPACE[int(cascade)] * vec4(fragment.position, 1.0);
	coords.w = coords.z;
	coords.z = float(cascade);
	shadow = filterPCF(coords);

	return clamp(shadow, 0.1, 1.0);
}
*/

void main(void)
{
	float height = texture(DISPLACEMENT, fragment.texcoord).r;
	vec3 normal = texture(NORMALMAP, fragment.texcoord).rgb;
	normal = (normal * 2.0) - 1.0;
	normal = normalize(normal);
	
	vec3 bump = texture(DETAILMAP, 100.0 * fragment.texcoord).rbg;
	bump = (bump * 2.0) - 1.0;
	bump = normalize(bump);

	vec3 tangent = normalize(cross(normal, vec3(0.0, 0.0, 1.0)));
	vec3 bitangent = normalize(cross(tangent, normal));
	mat3 orthobasis = mat3(tangent, normal, bitangent);
	vec3 detail_normal = orthobasis * bump;

	float slope = 1.0 - normal.y;
	slope = smoothstep(0.2, 0.6, slope);
	
	normal = mix(normal, detail_normal, slope);

	vec2 sitecoord = fragment.position.xz - SITE_OFFSET;
	float dirtlevel = texture(SITEMASKS, sitecoord / SITE_SCALE).r;
	
	vec3 stone = texture(STONEMAP, 100.0 * fragment.texcoord).rgb;
	vec3 sand = texture(SANDMAP, 200.0 * fragment.texcoord).rgb;
	vec3 grass = texture(GRASSMAP, 400.0 * fragment.texcoord).rgb;
	vec3 gravel = texture(GRAVELMAP, 800.0 * fragment.texcoord).rgb;
	
	//vec3 color = mix(sand, GRASS_COLOR * grass, smoothstep(0.1, 0.11, height));
	//vec3 color = vec3(0.796, 0.88, 0.512) * grass;
	vec3 color = GRASS_COLOR * grass;
	color = mix(color, gravel, dirtlevel);
	color = mix(color, stone, slope);

	// terrain lighting
	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	float diffuse = max(0.0, dot(normal, SUN_POS));

	vec3 scatteredlight = lightcolor * diffuse;
	color = mix(min(color * scatteredlight, vec3(1.0)), color, 0.5);

	fcolor = vec4(fog(color, distance(CAM_POS, fragment.position)), 1.0);
}

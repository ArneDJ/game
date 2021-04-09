#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	float zclipspace;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;
layout(binding = 1) uniform sampler2D NORMALMAP;
layout(binding = 2) uniform sampler2D STONEMAP;
layout(binding = 3) uniform sampler2D SANDMAP;
layout(binding = 4) uniform sampler2D STONE_NORMAL;

layout(binding = 10) uniform sampler2DArrayShadow SHADOWMAP;

// atmosphere
uniform vec3 CAM_POS;
uniform vec3 SUN_POS;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;

// shadows
uniform vec4 SPLIT;
uniform mat4 SHADOWSPACE[4];
uniform bool SHOW_CASCADES;

vec3 fog(vec3 color, float dist)
{
	float amount = 1.0 - exp(-dist * FOG_FACTOR);
	return mix(color, FOG_COLOR, amount );
}

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

void main(void)
{
	float height = texture(DISPLACEMENT, fragment.texcoord).r;
	vec3 normal = texture(NORMALMAP, fragment.texcoord).rgb;
	normal = (normal * 2.0) - 1.0;
	normal = normalize(normal);

	float slope = 1.0 - normal.y;
	//color = mix(rivercolor, color, 0.9);
	
	vec3 stone = texture(STONEMAP, 100.0 * fragment.texcoord).rgb;
	vec3 sand = texture(SANDMAP, 200.0 * fragment.texcoord).rgb;

	vec3 bump = texture(STONE_NORMAL, 100.0 * fragment.texcoord).rgb;
	bump = (bump * 2.0) - 1.0;
	bump = vec3(bump.x, bump.z, bump.y);
	bump = normalize(bump);
	
	vec3 color = mix(sand, stone, slope);
	normal = mix(normal, normalize(normal + bump), slope);

	if (SHOW_CASCADES == true) {
		vec3 cascade = vec3(0, 0, 0);
		if (fragment.zclipspace < SPLIT.x) {
			cascade = vec3(1.0, 0.0, 0.0);
		} else if (fragment.zclipspace < SPLIT.y) {
			cascade = vec3(0.0, 1.0, 0.0);
		} else if (fragment.zclipspace < SPLIT.z) {
			cascade = vec3(0.0, 0.0, 1.0);
		} else if (fragment.zclipspace < SPLIT.w) {
			cascade = vec3(1.0, 0.0, 1.0);
		}
		color = mix(color, cascade, 0.5);
	}

	// terrain lighting
	const vec3 ambient = vec3(0.5, 0.5, 0.5);
	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	float diffuse = max(0.0, dot(normal, SUN_POS));

	float shadow = shadow_coef();

	vec3 scatteredlight = lightcolor * diffuse * shadow;
	color = mix(min(color * scatteredlight, vec3(1.0)), color, 0.5);

	fcolor = vec4(fog(color, distance(CAM_POS, fragment.position)), 1.0);
}

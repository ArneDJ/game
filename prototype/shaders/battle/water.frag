#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	vec4 clip;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;
layout(binding = 1) uniform sampler2D NORMALMAP;
layout(binding = 2) uniform sampler2D DEPTHMAP;

layout(binding = 10) uniform sampler2DArrayShadow SHADOWMAP;

// atmosphere
uniform vec3 SUN_POS;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;
// camera
uniform vec3 CAM_POS;
uniform float NEAR_CLIP;
uniform float FAR_CLIP;
uniform float SCREEN_WIDTH;
uniform float SCREEN_HEIGHT;

// shadows
uniform vec4 SPLIT;
uniform mat4 SHADOWSPACE[4];

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
	if (fragment.clip.z < SPLIT.x) {
		cascade = 0;
	} else if (fragment.clip.z < SPLIT.y) {
		cascade = 1;
	} else if (fragment.clip.z < SPLIT.z) {
		cascade = 2;
	} else if (fragment.clip.z < SPLIT.w) {
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
	vec3 color = vec3(0.4, 0.5, 0.6);

	vec2 ndc = (fragment.clip.xy / fragment.clip.w) / 2.0 + 0.5;
	float near = NEAR_CLIP;
	float far = FAR_CLIP;
	float edge_softness = 10.5;
	float depth = texture(DEPTHMAP, ndc).r;
	float floor_dist = 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
	depth = gl_FragCoord.z;
	float water_dist = 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
	float waterdepth = floor_dist - water_dist;
	waterdepth = clamp(waterdepth / edge_softness, 0.0, 0.5);

	fcolor = vec4(fog(color, distance(CAM_POS, fragment.position)), waterdepth);
}

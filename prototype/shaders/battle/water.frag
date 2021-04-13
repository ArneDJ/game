#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	float zclipspace;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;
layout(binding = 1) uniform sampler2D NORMALMAP;

layout(binding = 10) uniform sampler2DArrayShadow SHADOWMAP;

// atmosphere
uniform vec3 CAM_POS;
uniform vec3 SUN_POS;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;

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
	vec3 color = vec3(0.0, 0.0, 1.0);

	fcolor = vec4(fog(color, distance(CAM_POS, fragment.position)), 1.0);
}
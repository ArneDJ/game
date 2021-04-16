#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	vec4 clip;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;
layout(binding = 2) uniform sampler2D DEPTHMAP;
layout(binding = 4) uniform sampler2D NORMALMAP;

layout(binding = 10) uniform sampler2DArrayShadow SHADOWMAP;

// atmosphere
uniform vec3 SUN_POS;
uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;
uniform vec2 WIND_DIR;
uniform float TIME;
// camera
uniform vec3 CAM_POS;
uniform float NEAR_CLIP;
uniform float FAR_CLIP;

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

vec3 do_specular(vec3 eyedir, vec3 lightdir, vec3 normal)
{
	float shinedamp = 20.0;
	float reflectivity = 0.4;
	vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	vec3 reflected = reflect(-lightdir, normal);
	float spec = dot(reflected , eyedir);
	spec = max(spec, 0.0);
	spec = pow(spec, shinedamp);

	return spec * reflectivity * lightcolor;
}

vec3 do_diffuse(vec3 lightdir, vec3 normal)
{
	vec2 lightbias = vec2(1.0, 1.0);
	vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	float brightness = max(dot(lightdir, normal), 0.0);

	return (lightcolor * lightbias.x) + (brightness * lightcolor * lightbias.y);
}

void main(void)
{
	vec3 color = vec3(0.7, 0.8, 0.9);
	vec3 shallowcolor = vec3(0.8, 0.95, 1.0);

	vec3 normal = texture(NORMALMAP, 0.05*fragment.position.xz + (0.1*TIME * WIND_DIR)).rbg;
	normal = (normal * 2.0) - 1.0;
	normal = normalize(normal);

	vec2 ndc = (fragment.clip.xy / fragment.clip.w) / 2.0 + 0.5;
	float near = NEAR_CLIP;
	float far = FAR_CLIP;
	float edge_softness = 10.5;
	float depth = texture(DEPTHMAP, ndc).r;
	float floor_dist = 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
	float water_dist = 2.0 * near * far / (far + near - (2.0 * gl_FragCoord.z - 1.0) * (far - near));
	float waterdepth = floor_dist - water_dist;

	color = mix(shallowcolor, color, clamp(waterdepth/40.0, 0.0, 1.0));

	waterdepth = clamp(waterdepth / edge_softness, 0.0, 0.5);

	vec3 eyedir = normalize(CAM_POS - fragment.position);
	vec3 spec = do_specular(eyedir, SUN_POS, normal);
	vec3 diff = do_diffuse(eyedir, normal);

	color = mix(color * diff, color, 0.8);
	color += spec;

	fcolor = vec4(fog(color, distance(CAM_POS, fragment.position)), waterdepth);
}

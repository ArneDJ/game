#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
	vec4 clip;
} fragment;

out vec4 fcolor;

layout(binding = 20) uniform sampler2D DEPTHMAP;
uniform sampler2D WATER_BUMPMAP;
uniform sampler2D DISPLACEMENT;

uniform vec3 FOG_COLOR;
uniform float FOG_FACTOR;
uniform float TIME;
uniform vec3 SUN_POS;
//
uniform vec3 CAM_POS;
uniform float NEAR_CLIP;
uniform float FAR_CLIP;

vec3 fog(vec3 color, float dist)
{
	float amount = 1.0 - exp(-dist * FOG_FACTOR);
	return mix(color, FOG_COLOR, amount );
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
	vec3 color = vec3(0.7, 0.8, 0.9) * vec3(0.3);
	vec3 shallowcolor = vec3(0.8, 0.95, 1.0) * vec3(0.4);

	vec3 normal = texture(WATER_BUMPMAP, 0.05*fragment.position.xz + (0.1*TIME * vec2(0.5, 0.5))).rbg;
	normal = (normal * 2.0) - 1.0;
	normal = normalize(normal);

	vec2 ndc = (fragment.clip.xy / fragment.clip.w) / 2.0 + 0.5;
	float near = NEAR_CLIP;
	float far = FAR_CLIP;
	float depth = texture(DEPTHMAP, ndc).r;
	float floor_dist = 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
	float water_dist = 2.0 * near * far / (far + near - (2.0 * gl_FragCoord.z - 1.0) * (far - near));
	// TODO fix z-fighting
	float waterdepth = floor_dist - water_dist;

	float shallowness = clamp(waterdepth/30.0, 0.0, 1.0);
	float height = texture(DISPLACEMENT, fragment.texcoord).r;

	color = mix(color, shallowcolor, smoothstep(0.3, 0.4, height));

	vec3 eyedir = normalize(CAM_POS - fragment.position);
	vec3 spec = do_specular(eyedir, SUN_POS, normal);
	vec3 diff = do_diffuse(eyedir, normal);

	color = mix(color * diff, color, 0.8);
	color += spec;

	float alpha = clamp(waterdepth / 1.5, 0.0, 1.0);
	/*
	if (distance(CAM_POS, fragment.position) > 400.0) {
		alpha = 1.0;
	}
	*/

	fcolor = vec4(fog(color, distance(CAM_POS, fragment.position)), alpha);

	float gamma = 1.2;
	fcolor.rgb = pow(fcolor.rgb, vec3(1.0/gamma));
}

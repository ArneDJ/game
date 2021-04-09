#version 430 core

in vec3 texcoords;

out vec4 fcolor;

uniform vec3 ZENITH_COLOR;
uniform vec3 HORIZON_COLOR;
uniform vec3 SUN_POS;
uniform bool CLOUDS_ENABLED;

layout (binding = 0) uniform sampler2D CLOUD_COLOR;
layout (binding = 1) uniform sampler2D CLOUD_ALPHA;

vec3 sun_color(vec3 pos, float expo)
{
	float sun = clamp(dot(SUN_POS, pos), 0.0, 1.0);
	vec3 color = 0.8 * vec3(1.0, 0.6, 0.1) * pow(sun, expo);

	return color;
}

void main(void)
{
	vec3 position = normalize(texcoords);

	float dist = clamp(1.5 * position.y, 0.0, 1.0);
	vec3 color = mix(HORIZON_COLOR, ZENITH_COLOR, dist);

	// add sun to the sky
	color += sun_color(position, 500.0);

	// add clouds to the sky if enabled
	if (CLOUDS_ENABLED) {
		vec2 coords = gl_FragCoord.xy;
		coords.x /= 1920.0;
		coords.y /= 1080.0;
		float alpha = texture(CLOUD_ALPHA, coords).r;
		vec3 cloudcolor = texture(CLOUD_COLOR, coords).rgb;

		cloudcolor = mix(cloudcolor * HORIZON_COLOR, cloudcolor, dist);
		color = mix(color, cloudcolor, alpha);
	}

	fcolor = vec4(color, 1.0);
}

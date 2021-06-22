#version 430 core

in vec3 texcoords;

out vec4 fcolor;

uniform vec3 ZENITH_COLOR;
uniform vec3 HORIZON_COLOR;
uniform vec3 SUN_POS;
uniform vec3 AMBIANCE_COLOR;
uniform float SCREEN_WIDTH;
uniform float SCREEN_HEIGHT;
uniform bool CLOUDS_ENABLED;
// fog
uniform float FOG_FACTOR;
uniform vec3 FOG_COLOR;

layout (binding = 0) uniform sampler2D CLOUDMAP;

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
		coords.x /= SCREEN_WIDTH;
		coords.y /= SCREEN_HEIGHT;
		vec4 cloud = texture(CLOUDMAP, coords);
		float alpha = cloud.a;
		vec3 cloudcolor = cloud.rgb;

		cloudcolor = mix(cloudcolor * HORIZON_COLOR, cloudcolor, dist);
		color = mix(color, cloudcolor, alpha);
	}
		
	color = mix(color, FOG_COLOR, FOG_FACTOR);

	fcolor = vec4(color, 1.0);

	fcolor.rgb *= AMBIANCE_COLOR;
}

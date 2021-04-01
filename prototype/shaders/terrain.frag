#version 430 core

in TESSEVAL {
	vec3 position;
	vec2 texcoord;
} fragment;

out vec4 fcolor;

layout(binding = 0) uniform sampler2D DISPLACEMENT;
layout(binding = 1) uniform sampler2D NORMALMAP;

void main(void)
{
	float height = texture(DISPLACEMENT, fragment.texcoord).r;
	vec3 normal = texture(NORMALMAP, fragment.texcoord).rgb;
	//normal = (normal * 2.0) - 1.0;
	//normal = normalize(normal);

	vec3 color = vec3(0.5, 0.5, 0.5);

	// terrain lighting
	const vec3 lightdirection = vec3(0.5, 0.93, 0.1);
	const vec3 ambient = vec3(0.5, 0.5, 0.5);
	const vec3 lightcolor = vec3(1.0, 1.0, 1.0);
	float diffuse = max(0.0, dot(normal, lightdirection));
	vec3 scatteredlight = lightcolor * diffuse;
	color = mix(min(color * scatteredlight, vec3(1.0)), color, 0.5);

	fcolor = vec4(normal, 1.0);
}

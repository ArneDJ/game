#version 430 core

in vec2 texcoords;

out vec4 fcolor;

uniform sampler2D text;

void main()
{    
	vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, texcoords).r);

	fcolor = vec4(vec3(1.0, 1.0, 1.0), 1.0) * sampled;
	//fcolor = vec4(1.0, 0.0, 1.0, 1.0);
} 

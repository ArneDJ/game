#version 430 core

out vec4 fcolor;
  
in vec2 texcoords;

layout (binding = 0) uniform sampler2D CLOUD_COLOR;
layout (binding = 1) uniform sampler2D CLOUD_ALPHA;

void main()
{ 
	float alpha = texture(CLOUD_ALPHA, texcoords).r;
	vec3 color = texture(CLOUD_COLOR, texcoords).rgb;
	fcolor = vec4(color, alpha);
}

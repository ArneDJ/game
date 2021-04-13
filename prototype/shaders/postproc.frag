#version 430 core

out vec4 fcolor;
  
in vec2 texcoords;

layout (binding = 0) uniform sampler2D COLORMAP;
layout (binding = 1) uniform sampler2D DEPTHMAP;

void main()
{ 
	fcolor = texture(COLORMAP, texcoords);
	
	/*
	float depth = texture(DEPTHMAP, texcoords).r;
	float ndc = depth * 2.0 - 1.0; 
	float near = 0.1;
	float far = 100.0;
	depth = (2.0 * near * far) / (far + near - ndc * (far - near));
	depth /= far;
	fcolor = vec4(vec3(depth), 1.0);
	*/

	//fcolor = vec4(1.0, 0.0, 1.0, 1.0);
}

#version 430 core

layout(location = 0) in vec3 vposition;

uniform mat4 DEPTH_VP;
uniform mat4 MODEL;

void main(void)
{
	gl_Position = DEPTH_VP * MODEL * vec4(vposition, 1.0);
}

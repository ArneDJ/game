#version 430 core

layout(location = 0) in vec3 vposition;

out vec3 texcoords;

uniform mat4 P, V;

void main(void)
{
	mat4 boxview = mat4(mat3(V));
	vec4 position = P * boxview * vec4(vposition, 1.0);
	texcoords = vposition;

	gl_Position = position.xyww;
}  

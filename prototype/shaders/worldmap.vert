#version 430 core

layout(location = 0) in vec3 vposition;
layout(location = 1) in vec3 vnormal;
layout(location = 2) in vec2 vtexcoords;

out vec3 normal;
out vec2 texcoords;

uniform mat4 VP;
uniform mat4 MODEL;

void main(void)
{
	texcoords = vtexcoords;

	normal = normalize(mat3(transpose(inverse(MODEL))) * normalize(vnormal));
	gl_Position = VP * MODEL * vec4(vposition, 1.0);
}

#version 430 core

layout(location = 0) in vec3 vposition;
layout(location = 1) in vec3 vnormal;
layout(location = 2) in vec2 vtexcoords;
layout(location = 3) in ivec4 joints;
layout(location = 4) in vec4 weights;

out vec3 normal;
out vec2 texcoords;

layout(binding = 20) uniform samplerBuffer joint_matrix_tbo;

uniform mat4 VP;
uniform mat4 MODEL;

mat4 fetch_joint_matrix(int joint)
{
	int base_index = 4 * joint;

	mat4 matrix;
	matrix[0] = texelFetch(joint_matrix_tbo, base_index);
	matrix[1] = texelFetch(joint_matrix_tbo, base_index + 1);
	matrix[2] = texelFetch(joint_matrix_tbo, base_index + 2);
	matrix[3] = texelFetch(joint_matrix_tbo, base_index + 3);

	return matrix;
}


void main(void)
{
	mat4 skin = weights.x * fetch_joint_matrix(int(joints.x));
	skin += weights.y * fetch_joint_matrix(int(joints.y));
	skin += weights.z * fetch_joint_matrix(int(joints.z));
	skin += weights.w * fetch_joint_matrix(int(joints.w));

	normal = vnormal;
	texcoords = vtexcoords;
	gl_Position = VP * MODEL * skin * vec4(vposition, 1.0);
}

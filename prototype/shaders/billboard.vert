#version 430 core

layout(location = 0) in vec3 vposition;
layout(location = 1) in vec3 vnormal;
layout(location = 2) in vec2 vtexcoords;

out vec3 position;
out vec2 texcoords;

layout(binding = 10) uniform samplerBuffer TRANSFORMS; // for instanced rendering

uniform bool INSTANCED;
uniform mat4 VP;
uniform mat4 PROJECT;
uniform mat4 VIEW;
uniform mat4 MODEL;

void main(void)
{
	texcoords = vtexcoords;

	if (INSTANCED == true) {
		vec4 col[4];
		col[0] = texelFetch(TRANSFORMS, gl_InstanceID * 4);
		col[1] = texelFetch(TRANSFORMS, gl_InstanceID * 4 + 1);
		col[2] = texelFetch(TRANSFORMS, gl_InstanceID * 4 + 2);
		col[3] = texelFetch(TRANSFORMS, gl_InstanceID * 4 + 3);
		mat4 T = mat4(col[0], col[1], col[2], col[3]);
		mat4 MV = VIEW * T;
		float scale_x = length(vec3(col[0].x, col[0].y, col[0].z));
		float scale_y = length(vec3(col[1].x, col[1].y, col[1].z));
		float scale_z = length(vec3(col[2].x, col[2].y, col[2].z));
		// Column 0:
		MV[0][0] = 1;
		MV[0][1] = 0;
		MV[0][2] = 0;
		// Column 2:
		MV[2][0] = 0;
		MV[2][1] = 0;
		MV[2][2] = 1;

		vec4 worldpos = col[3];
		position = worldpos.xyz;
		gl_Position = PROJECT * MV * vec4(scale_x*vposition.x, vposition.y, scale_z*vposition.z, 1.0);
	} else {
		vec4 worldpos = MODEL * vec4(vposition, 1.0);
		position = worldpos.xyz;
		gl_Position = VP * worldpos;
	}
}

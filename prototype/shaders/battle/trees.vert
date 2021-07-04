#version 430 core

layout(location = 0) in vec3 vposition;
layout(location = 1) in vec3 vnormal;
layout(location = 2) in vec2 vtexcoords;

out vec3 normal;
out vec3 position;
out vec2 texcoords;

uniform samplerBuffer TRANSFORMS; // for instanced rendering

uniform bool INSTANCED;
uniform mat4 VP;
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
		normal = normalize(mat3(transpose(inverse(T))) * normalize(vnormal));
		vec4 worldpos = T * vec4(vposition, 1.0);
		position = worldpos.xyz;
		gl_Position = VP * worldpos;
	} else {
		normal = normalize(mat3(transpose(inverse(MODEL))) * normalize(vnormal));
		//gl_Position = VP * MODEL * vec4(vposition, 1.0);
		vec4 worldpos = MODEL * vec4(vposition, 1.0);
		position = worldpos.xyz;
		gl_Position = VP * worldpos;
	}
}

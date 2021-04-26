#version 430 core

layout(location = 0) in vec3 vposition;
layout(location = 1) in vec3 vnormal;
layout(location = 2) in vec2 vtexcoords;

out vec3 position;
out vec2 texcoords;
out float diffuse;

layout(binding = 1) uniform sampler2D TERRAIN_HEIGHTMAP;
layout(binding = 2) uniform sampler2D TERRAIN_NORMALMAP;
layout(binding = 10) uniform samplerBuffer TRANSFORMS; // for instanced rendering

uniform vec2 ROOT_OFFSET;
uniform vec2 CHUNK_SCALE;

uniform vec3 MAPSCALE;
uniform vec3 SUN_POS;
uniform mat4 VP;
uniform mat4 MODEL;

void main(void)
{
	texcoords = vtexcoords;

	vec4 col[4];
	col[0] = texelFetch(TRANSFORMS, gl_InstanceID * 4);
	col[1] = texelFetch(TRANSFORMS, gl_InstanceID * 4 + 1);
	col[2] = texelFetch(TRANSFORMS, gl_InstanceID * 4 + 2);
	col[3] = texelFetch(TRANSFORMS, gl_InstanceID * 4 + 3);
	mat4 T = mat4(col[0], col[1], col[2], col[3]);

	// the position of the grass is normalized so we first have to rescale it on the scale of the grass chunk
	vec4 translation = col[3];
	translation.x *= CHUNK_SCALE.x;
	translation.z *= CHUNK_SCALE.y;
	translation.x += ROOT_OFFSET.x;
	translation.z += ROOT_OFFSET.y;
	T[3] = translation;

	float slope = 1.0 - texture(TERRAIN_NORMALMAP, translation.xz / MAPSCALE.xz).y;

	vec4 worldpos = T * vec4(vposition, 1.0);

	// if slope is too steep grass won't grow on it
	if (slope > 0.15) {
		worldpos = vec4(0.0); // dirty way of not showing a vertex
	}

	position = worldpos.xyz;
	
	vec2 uv = position.xz / MAPSCALE.xz;

	float height = texture(TERRAIN_HEIGHTMAP, uv).r;

	worldpos.y += vposition.y + (MAPSCALE.y * height);
	position = worldpos.xyz;

	vec3 normal = texture(TERRAIN_NORMALMAP, uv).rgb;
	normal = (normal * 2.0) - 1.0;
	normal = normalize(normal);

	// terrain lighting
	diffuse = max(0.0, dot(normal, SUN_POS));

	gl_Position = VP * worldpos;
}

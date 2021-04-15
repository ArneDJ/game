#version 430 core

layout(quads, fractional_even_spacing, ccw) in;

out TESSEVAL {
	vec3 position;
	vec2 texcoord;
	vec4 clip;
} tesseval;

layout(binding = 6) uniform sampler2D DISPLACEMENT;
uniform mat4 VP;
uniform vec3 MAP_SCALE;

void main(void)
{
	vec4 p1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.y);
	vec4 p2 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.y);
	vec4 pos = mix(p1, p2, gl_TessCoord.x);

	tesseval.texcoord = pos.xz / MAP_SCALE.xz;

	pos.y = MAP_SCALE.y * (texture(DISPLACEMENT, tesseval.texcoord).r + texture(DISPLACEMENT, tesseval.texcoord).g);
	//pos.y = MAP_SCALE.y * 0.1;
	tesseval.position = pos.xyz;

	tesseval.clip = VP * pos;

	gl_Position = tesseval.clip;
}

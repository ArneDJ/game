#version 430 core

layout(vertices = 4) out;

uniform vec3 CAM_POS;

float lod(float dist)
{
	float tessfactor = smoothstep(600.0, 800.0, dist);
	return mix(4.0, 32.0, 1.0-tessfactor);
}

// add half vector
precise vec2 midpoint(vec2 a, vec2 b)
{
	return 0.5 * (a + b);
}

void main(void)
{
	if (gl_InvocationID == 0) {
		// tesselate based on distance to edges midpoint to prevent cracking
		vec2 e0 = midpoint(gl_in[0].gl_Position.xz, gl_in[1].gl_Position.xz); // 0
		vec2 e1 = midpoint(gl_in[0].gl_Position.xz, gl_in[2].gl_Position.xz); // 1
		vec2 e2 = midpoint(gl_in[2].gl_Position.xz, gl_in[3].gl_Position.xz); // 2
		vec2 e3 = midpoint(gl_in[1].gl_Position.xz, gl_in[3].gl_Position.xz); // 3

		gl_TessLevelOuter[0] = lod(distance(e0, CAM_POS.xz));
		gl_TessLevelOuter[1] = lod(distance(e1, CAM_POS.xz));
		gl_TessLevelOuter[2] = lod(distance(e2, CAM_POS.xz));
		gl_TessLevelOuter[3] = lod(distance(e3, CAM_POS.xz));
		gl_TessLevelInner[0] = mix(gl_TessLevelOuter[0], gl_TessLevelOuter[3], 0.5);
		gl_TessLevelInner[1] = mix(gl_TessLevelOuter[2], gl_TessLevelOuter[1], 0.5);
	}

	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}

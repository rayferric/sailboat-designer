#version 460

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_TexCoord;
layout(location = 2) in vec3 in_Normal;
layout(location = 3) in vec3 in_Tangent;

layout(location = 0) out vec2 v_TexCoord;
// layout(location = 1) out mat3 v_TBN;
layout(location = 1) out vec3 v_Normal;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
	float time;
} u_Frame;

layout(std140, binding = 1) uniform Entity {
	mat4 modelMat;
	vec4 tint;
} u_Entity;

void main() {
	v_TexCoord = in_TexCoord;

	// vec3 T = normalize(mat3(u_Entity.modelMat) * in_Tangent);
   	// vec3 N = normalize(mat3(u_Entity.modelMat) * in_Normal);
	// vec3 B = normalize(cross(N, T));
	// v_TBN = transpose(mat3(T, B, N));
	v_Normal = normalize(mat3(u_Entity.modelMat) * in_Normal);

	vec4 worldPos = u_Entity.modelMat * vec4(in_Position, 1.0);
    gl_Position = u_Frame.projMat * u_Frame.viewMat * worldPos;
}

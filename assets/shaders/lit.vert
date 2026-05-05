#version 460

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_TexCoord;
layout(location = 2) in vec3 in_Normal;
layout(location = 3) in vec3 in_Tangent;

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec3 v_WorldPos;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
	float time;
	vec4 sunDir;
	vec4 sunColor;
	mat4 lightVP;
} u_Frame;

layout(std140, binding = 1) uniform Entity {
	mat4 modelMat;
	vec4 tint;
} u_Entity;

void main() {
	v_TexCoord = in_TexCoord;
	v_Normal = normalize(mat3(u_Entity.modelMat) * in_Normal);

	vec4 worldPos = u_Entity.modelMat * vec4(in_Position, 1.0);
	v_WorldPos = worldPos.xyz;

    gl_Position = u_Frame.projMat * u_Frame.viewMat * worldPos;
}

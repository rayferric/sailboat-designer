#version 460

layout(location = 0) in vec3 in_Position;

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
	gl_Position = u_Frame.lightVP * u_Entity.modelMat * vec4(in_Position, 1.0);
}

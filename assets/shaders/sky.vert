#version 460

layout(location = 0) out vec3 v_Dir;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
	vec4 timeAndYaw;
	vec4 sunDir;
	vec4 sunColor;
	mat4 lightVP;
} u_Frame;

void main() {
	// Fullscreen triangle from gl_VertexID (0,1,2)
	vec2 ndc = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;

	// Reconstruct world-space view direction from NDC
	mat4 invProj = inverse(u_Frame.projMat);
	mat3 invView = transpose(mat3(u_Frame.viewMat));

	vec4 viewPos = invProj * vec4(ndc, 1.0, 1.0);
	vec3 dir = invView * (viewPos.xyz / viewPos.w);
	
	v_Dir = dir;

	gl_Position = vec4(ndc, 1.0, 1.0);
}
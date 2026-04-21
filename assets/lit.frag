#version 460

layout(location = 0) out vec4 out_Color;

layout(location = 0) in vec2 v_TexCoord;
// layout(location = 1) in mat3 v_TBN;
layout(location = 1) in vec3 v_Normal;

layout(std140, binding = 1) uniform Entity {
	mat4 modelMat;
	vec4 tint;
} u_Entity;

layout(std140, binding = 2) uniform Mat {
	vec4 color;
} u_Material;

layout(binding = 0) uniform sampler2D tex_Color;

void main() {
	vec3 albedo = (u_Material.color * texture(tex_Color, v_TexCoord)).xyz;
	albedo = mix(albedo, u_Entity.tint.xyz, u_Entity.tint.w);

	// vec3 N = normalize(v_TBN * vec3(0.0, 0.0, 1.0));
	vec3 N = normalize(v_Normal);
	vec3 L = normalize(vec3(1.0, 1.0, 1.0));

	float diffuse = max(dot(N, L), 0.0);
	float ambient = 0.3;
	float light = ambient + diffuse;

	vec3 color = albedo * light;
	out_Color = vec4(color, 1.0);
}

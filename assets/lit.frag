#version 460

// PBR lighting — Cook-Torrance GGX + sky-based environment lighting.
// Follows learnopengl.com/PBR/Lighting with approximate IBL from the HDR
// equirect (one sample each for diffuse and specular, no prefiltered cubemap).

layout(location = 0) out vec4 out_Color;

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec3 v_WorldPos;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
	float time;
	vec4 sunDir;    // w = 0 (direction, not position)
	vec4 sunColor;  // w = 1 (rgb = linear HDR radiance)
} u_Frame;

layout(std140, binding = 1) uniform Entity {
	mat4 modelMat;
	vec4 tint;
} u_Entity;

layout(std140, binding = 2) uniform Mat {
	vec4 color;
	vec4 pbr;  // x = metallic, y = roughness
} u_Material;

layout(binding = 0) uniform sampler2D tex_Color;
layout(binding = 1) uniform sampler2D tex_Sky;

const float PI = 3.14159265359;

// Equirect sky sampling — matches sky.frag and water.frag.
vec2 equirect_uv(vec3 v) {
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= vec2(1.0 / (2.0 * PI), 1.0 / PI);
	uv += 0.5;
	return uv;
}

vec3 sample_sky(vec3 dir) {
	return texture(tex_Sky, equirect_uv(normalize(dir))).rgb;
}

// GGX normal distribution
float D_GGX(float NdotH, float rough) {
	float a  = rough * rough;
	float a2 = a * a;
	float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
	return a2 / max(PI * d * d, 1e-6);
}

// Schlick-GGX geometry term (direct lighting variant)
float G_Schlick(float NdotX, float rough) {
	float k = (rough + 1.0) * (rough + 1.0) / 8.0;
	return NdotX / max(NdotX * (1.0 - k) + k, 1e-6);
}

float G_Smith(float NdotV, float NdotL, float rough) {
	return G_Schlick(NdotV, rough) * G_Schlick(NdotL, rough);
}

// Fresnel-Schlick
vec3 F_Schlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
	// Material
	vec3 albedo    = (u_Material.color * texture(tex_Color, v_TexCoord)).rgb;
	albedo         = mix(albedo, u_Entity.tint.rgb, u_Entity.tint.a);
	float metallic = u_Material.pbr.x;
	float rough    = max(u_Material.pbr.y, 0.05);

	// Camera position from view matrix
	mat3 Rv = mat3(u_Frame.viewMat);
	vec3 camPos = -transpose(Rv) * vec3(u_Frame.viewMat[3]);

	vec3 N = normalize(v_Normal);
	vec3 V = normalize(camPos - v_WorldPos);
	vec3 L = normalize(u_Frame.sunDir.xyz);
	vec3 H = normalize(V + L);
	vec3 R = reflect(-V, N);

	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 1e-4);
	float NdotH = max(dot(N, H), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	// Base reflectivity: 0.04 for dielectrics, albedo for metals
	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	// --- Direct sun (Cook-Torrance) ---
	vec3 F_sun = F_Schlick(HdotV, F0);
	float D    = D_GGX(NdotH, rough);
	float G    = G_Smith(NdotV, NdotL, rough);

	vec3 specular_direct = (D * G * F_sun) / max(4.0 * NdotV * NdotL, 1e-4);
	vec3 kd_direct = (1.0 - F_sun) * (1.0 - metallic);
	vec3 diffuse_direct = kd_direct * albedo / PI;

	vec3 Lo = (diffuse_direct + specular_direct) * u_Frame.sunColor.rgb * NdotL;

	// --- Sky environment (approximate IBL) ---
	// Diffuse: sky radiance in normal direction as irradiance approximation.
	vec3 F_env = F_Schlick(NdotV, F0);
	vec3 kd_env = (1.0 - F_env) * (1.0 - metallic);
	vec3 diffuse_env = kd_env * albedo * sample_sky(N);

	// Specular: sky in reflection direction. Rougher surfaces blend toward
	// diffuse direction (cheap substitute for prefiltered mip levels).
	vec3 R_rough = mix(R, N, rough * rough);
	vec3 specular_env = F_env * sample_sky(R_rough);

	vec3 ambient = diffuse_env + specular_env;

	vec3 finalColor = Lo + ambient;

	// Reinhard tonemap + gamma — same pipeline as sky.frag and water.frag.
	finalColor = finalColor / (finalColor + 1.0);
	finalColor = pow(finalColor, vec3(1.0 / 2.2));

	out_Color = vec4(finalColor, 1.0);
}

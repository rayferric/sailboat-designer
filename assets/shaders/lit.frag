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
	vec4 timeAndYaw; // x: time, y: envYaw
	vec4 sunDir;     // w = 0 (direction, not position)
	vec4 sunColor;   // w = 1 (rgb = linear HDR radiance)
	mat4 lightVP;    // sun's view-projection for shadow mapping
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
layout(binding = 2) uniform sampler2D tex_Shadow;

const float PI = 3.14159265359;

// Equirect sky sampling — matches sky.frag and water.frag.
vec2 equirect_uv(vec3 v) {
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= vec2(1.0 / (2.0 * PI), 1.0 / PI);
	uv += 0.5;
	return uv;
}

vec3 sample_sky(vec3 dir, float roughness) {
	float envYaw = u_Frame.timeAndYaw.y;
	float c = cos(envYaw);
	float s = sin(envYaw);
	vec3 samplingDir = mat3(c, 0, s, 0, 1, 0, -s, 0, c) * dir;
	float max_lod = float(textureQueryLevels(tex_Sky) - 1);
	return textureLod(tex_Sky, equirect_uv(normalize(samplingDir)), roughness * max_lod).rgb;
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

// 3x3 PCF shadow lookup. Returns 1.0 = fully lit, 0.0 = fully shadowed.
// Bias scaled by surface angle to sun mitigates shadow acne on grazing slopes.
float compute_shadow(vec3 worldPos, float NdotL) {
	vec4 lp = u_Frame.lightVP * vec4(worldPos, 1.0);
	vec3 proj = lp.xyz / lp.w;
	proj = proj * 0.5 + 0.5;
	if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
		return 1.0;
	}
	float bias = max(0.003 * (1.0 - NdotL), 0.0005);
	float currentDepth = proj.z - bias;
	float result = 0.0;
	// 5x5 PCF with 1.5-texel spacing — softer penumbra than tight 3x3.
	vec2 texelSize = 1.5 / vec2(textureSize(tex_Shadow, 0));
	for (int x = -2; x <= 2; ++x) {
		for (int y = -2; y <= 2; ++y) {
			float pcfDepth = texture(tex_Shadow, proj.xy + vec2(x, y) * texelSize).r;
			result += currentDepth > pcfDepth ? 0.0 : 1.0;
		}
	}
	return result / 25.0;
}

void main() {
	// Material
	vec4 tex_color = texture(tex_Color, v_TexCoord);
	if (tex_color.a < 0.01) {
		discard;
	}
	vec3 albedo    = (u_Material.color * tex_color).rgb;
	albedo         = mix(albedo, u_Entity.tint.rgb, u_Entity.tint.a);
	float metallic = u_Material.pbr.x;
	// Floor raised to make hull/sail materials read as matte rather than
	// plasticky — most glTF assets here ship with low roughness factors.
	float rough    = max(u_Material.pbr.y, 0.0);

	// Camera position from view matrix
	mat3 Rv = mat3(u_Frame.viewMat);
	vec3 camPos = -transpose(Rv) * vec3(u_Frame.viewMat[3]);

	vec3 N = normalize(v_Normal);
	vec3 V = normalize(camPos - v_WorldPos);
	vec3 L = normalize(u_Frame.sunDir.xyz);
	vec3 H = normalize(V + L);
	vec3 R = reflect(-V, N);

	// // debug: set albedo to view space normal
	// albedo = mat3(u_Frame.viewMat) * N;

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

	float shadow = compute_shadow(v_WorldPos, NdotL);
	vec3 Lo = (diffuse_direct + specular_direct) * u_Frame.sunColor.rgb * NdotL * shadow;

	// --- Sky environment (approximate IBL) ---
	// Diffuse: sky radiance in normal direction as irradiance approximation.
	vec3 F_env = F_Schlick(NdotV, F0);
	vec3 kd_env = (1.0 - F_env) * (1.0 - metallic);
	vec3 diffuse_env = kd_env * albedo * sample_sky(N, 1.0);

	// Specular: sky in reflection direction. LOD handles the blurring
	// based on the roughness of the surface.
	vec3 specular_env = F_env * sample_sky(R, rough);

	// Cheap fake AO: surfaces facing down get less sky ambient than ones facing
	// up. Approximates the fact that geometry below typically blocks the sky.
	float ao = mix(0.4, 1.0, clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
	vec3 ambient = (diffuse_env + specular_env) * ao;

	vec3 finalColor = Lo + ambient;

	// // Underwater fog. Geometry below the water plane (y = 0) fades toward the
	// // deep water color exponentially with depth, so the submerged part of the
	// // hull dissolves into the water instead of being plainly visible through
	// // the translucent surface. Color matches water.frag's deepWaterColor.
	// if (v_WorldPos.y < 0.0) {
	// 	float depth = -v_WorldPos.y;
	// 	float fog = 1.0 - exp(-depth * 1.8);
	// 	vec3 deepWaterColor = vec3(0.01, 0.04, 0.08);
	// 	finalColor = mix(finalColor, deepWaterColor, fog);
	// }

	out_Color = vec4(finalColor, tex_color.a);
}

#version 460

layout(location = 0) in vec3 v_Dir;

layout(location = 0) out vec4 out_Color;

layout(binding = 0) uniform sampler2D tex_Sky;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
	vec4 timeAndYaw; // x: time, y: envYaw
	vec4 sunDir;     // w = 0 (direction, not position)
	vec4 sunColor;   // w = 1 (rgb = linear HDR radiance)
	mat4 lightVP;    // sun's view-projection for shadow mapping
} u_Frame;

const float PI = 3.14159265359;

vec2 sample_equirect(vec3 v) {
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
    return textureLod(tex_Sky, sample_equirect(normalize(samplingDir)), roughness * max_lod).rgb;
}

void main() {
	vec3 dir = normalize(v_Dir);
	vec3 hdr = sample_sky(dir, 0.0);

	out_Color = vec4(hdr, 1.0);
}
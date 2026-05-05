#version 460

layout(location = 0) in vec3 v_Dir;

layout(location = 0) out vec4 out_Color;

layout(binding = 0) uniform sampler2D tex_Sky;

const float PI = 3.14159265359;

vec2 sample_equirect(vec3 v) {
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= vec2(1.0 / (2.0 * PI), 1.0 / PI);
	uv += 0.5;
	return uv;
}

void main() {
	vec3 dir = normalize(v_Dir);
	vec3 hdr = texture(tex_Sky, sample_equirect(dir)).rgb;

	// Reinhard tonemap + gamma
	vec3 mapped = hdr / (hdr + 1.0);
	mapped = pow(mapped, vec3(1.0 / 2.2));

	out_Color = vec4(mapped, 1.0);
}
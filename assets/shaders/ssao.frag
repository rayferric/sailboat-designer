#version 460

in vec2 v_Uv;
out vec4 out_Color;

layout(binding = 0) uniform sampler2D tex_Color;
layout(binding = 1) uniform sampler2D tex_Depth;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
	vec4 timeAndYaw;
	vec4 sunDir;
	vec4 sunColor;
	mat4 lightVP;
} u_Frame;

float interleaved_gradient_noise(vec2 uv) {
    vec2 p = fract(uv * vec2(0.1031, 0.1030));
    p += dot(p, p.yx + 33.33);
    return fract((p.x + p.y) * p.x);
}

vec3 get_view_pos(vec2 uv, float depth, mat4 inv_proj) {
	vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 view = inv_proj * clip;
	return view.xyz / view.w;
}

vec2 view_to_uv(vec3 view_pos) {
	vec4 proj = u_Frame.projMat * vec4(view_pos, 1.0);
	return (proj.xy / proj.w) * 0.5 + 0.5;
}

// reconstructs view-space normal from depth buffer derivatives
vec3 reconstruct_normal(vec3 pos) {
	vec3 dx = dFdx(pos);
	vec3 dy = dFdy(pos);
	return normalize(cross(dx, dy));
}

void main() {
	vec3 color = texture(tex_Color, v_Uv).rgb;
	float depth = texture(tex_Depth, v_Uv).r;

	if (depth >= 1.0) {
		out_Color = vec4(color, 1.0);
		return;
	}

	mat4 inv_proj = inverse(u_Frame.projMat);
	vec3 frag_pos = get_view_pos(v_Uv, depth, inv_proj);
	vec3 normal = reconstruct_normal(frag_pos);

	float ao = 0.0;
	const int samples = 16;
	const float radius = 0.2;  // view-space units
	const float bias = 0.002;  // prevents self-occlusion acne

	float angle = interleaved_gradient_noise(gl_FragCoord.xy) * 6.28318;

	// build tangent frame from normal so samples stay in the hemisphere
	vec3 rand_vec = vec3(cos(angle), sin(angle), 0.0);
	vec3 tangent   = normalize(rand_vec - normal * dot(rand_vec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 tbn = mat3(tangent, bitangent, normal);

	for (int i = 0; i < samples; ++i) {
		float r = (float(i) + 0.5) / float(samples);
		float a = float(i) * 2.39996 + angle;

		// hemisphere sample, weighted toward center
		vec3 hemisphere = vec3(cos(a), sin(a), sqrt(1.0 - r * r)) * r;
		vec3 sample_pos = frag_pos + tbn * hemisphere * radius;

		vec2 sample_uv  = view_to_uv(sample_pos);
		float sample_depth = texture(tex_Depth, sample_uv).r;
		vec3  sample_actual = get_view_pos(sample_uv, sample_depth, inv_proj);

		// range check prevents halos at depth discontinuities
		float range_check = smoothstep(0.0, 1.0, radius / abs(frag_pos.z - sample_actual.z));

		// in view space, closer to camera = less negative z
		if (sample_actual.z >= sample_pos.z + bias) {
			ao += range_check;
		}
	}

	ao = 1.0 - (ao / float(samples));
    ao = pow(ao, 1.5);
	out_Color = vec4(color * ao, 1.0);
}
#pragma once

#include "../pch.hpp"

const char *spherical_blur_shd = R"(
#version 430 core
out vec4 frag_color;
in vec2 uv;

uniform sampler2D hdr_map;
uniform float roughness;
uniform float resolution;

const uint samples = 1024u;
const float pi = 3.14159265359;

float vdc(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint n) {
	return vec2(float(i)/float(n), vdc(i));
}

vec3 importance_sample_ggx(vec2 u, vec3 n, float rough) {
	float a = rough * rough;
	float phi = 2.0 * pi * u.x;
	float cos_theta = sqrt((1.0 - u.y) / (1.0 + (a*a - 1.0) * u.y));
	float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	vec3 h = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
	vec3 up = abs(n.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
	vec3 tangent = normalize(cross(up, n));
	vec3 bitangent = cross(n, tangent);
	return normalize(tangent * h.x + bitangent * h.y + n * h.z);
}

void main() {
	float theta = (uv.y - 0.5) * pi;
	float phi = (uv.x - 0.5) * 2.0 * pi;
	vec3 n = normalize(vec3(cos(theta) * sin(phi), sin(theta), cos(theta) * cos(phi)));
	
	vec3 color = vec3(0.0);
	float weight = 0.0;
	float a = roughness * roughness;

	for(uint i = 0u; i < samples; ++i) {
		vec2 u = hammersley(i, samples);
		
		// generate microfacet normal (halfway vector)
		float phi_h = 2.0 * pi * u.x;
		float cos_theta_h = sqrt((1.0 - u.y) / (1.0 + (a*a - 1.0) * u.y));
		float sin_theta_h = sqrt(1.0 - cos_theta_h * cos_theta_h);
		vec3 h_local = vec3(cos(phi_h) * sin_theta_h, sin(phi_h) * sin_theta_h, cos_theta_h);
		
		vec3 up = abs(n.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
		vec3 tx = normalize(cross(up, n));
		vec3 ty = cross(n, tx);
		vec3 h = normalize(tx * h_local.x + ty * h_local.y + n * h_local.z);
		
		vec3 l = normalize(2.0 * dot(n, h) * h - n);
		float n_dot_l = max(dot(n, l), 0.0);
		
		if(n_dot_l > 0.0) {
			// calculate lod to prevent pole artifacts and noise
			float d = (cos_theta_h * a * a - cos_theta_h) * cos_theta_h + 1.0;
			float pdf = (a * a * cos_theta_h * sin_theta_h) / (pi * d * d + 0.0001);
			float sa_sample = 1.0 / (float(samples) * pdf + 0.0001);
			float sa_texel = 4.0 * pi / (6.0 * resolution * resolution);
			float lod = roughness == 0.0 ? 0.0 : 0.5 * log2(sa_sample / sa_texel);

			vec2 sample_uv = vec2(atan(l.x, l.z) / (2.0 * pi) + 0.5, asin(l.y) / pi + 0.5);
			color += textureLod(hdr_map, sample_uv, lod).rgb * n_dot_l;
			weight += n_dot_l;
		}
	}
	frag_color = vec4(color / weight, 1.0);
}
)";

// screen quad shader
const char *quad_vtx_shd = R"(
#version 430 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 tex;
out vec2 uv;
void main() { uv = tex; gl_Position = vec4(pos, 1.0); }
)";

static void generate_spherical_mips(GLuint tex_id, int width, int height, int max_lods) {
	GLuint fbo, vao, vbo, prog;
	glGenFramebuffers(1, &fbo);
	
	float quad[] = { -1,1,0,0,1, -1,-1,0,0,0, 1,1,0,1,1, 1,-1,0,1,0 };
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

	auto compile = [](GLenum type, const char *src) {
		GLuint s = glCreateShader(type);
		glShaderSource(s, 1, &src, NULL);
		glCompileShader(s);
		return s;
	};

	prog = glCreateProgram();
	GLuint vs = compile(GL_VERTEX_SHADER, quad_vtx_shd);
	GLuint fs = compile(GL_FRAGMENT_SHADER, spherical_blur_shd);
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	std::cout << "Compiled HDRI LOD shader with program ID " << prog << std::endl;
	glUseProgram(prog);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	for (int i = 1; i < max_lods; ++i) {
		int mw = width >> i;
		int mh = height >> i;
		glViewport(0, 0, mw, mh);
		
		float roughness = (float)i / (float)(max_lods - 1) / 2.0; // halving is a DIRTY FIX
		glUniform1f(glGetUniformLocation(prog, "roughness"), roughness);
		glUniform1f(glGetUniformLocation(prog, "resolution"), (float)mw);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex_id);
		glUniform1i(glGetUniformLocation(prog, "hdr_map"), 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, i);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	glDeleteProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
	glDeleteFramebuffers(1, &fbo);
}
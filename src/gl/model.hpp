#pragma once

#include "../pch.hpp"

#include "./texture.hpp"
#include "./uniform_buffer.hpp"

struct vertex {
	float pos[3];
	float uv[2];
	float norm[3];
	float tangent[3];
};

class mesh {
public:
	GLuint vao;
	GLuint vbo;
	size_t num_verts;

	mesh();
	~mesh();
	mesh(mesh &&m) : vao(m.vao), vbo(m.vbo), num_verts(m.num_verts) {
		m.vao = m.vbo = 0;
	}

	void load(const std::vector<vertex> &verts);
};

struct material {
	glm::vec4 color;
	std::optional<texture> color_tex;
};

struct part {
	mesh mesh;
	material mat;
};

class model {
public:
	void load_from_glb(const std::filesystem::path &path);
	void draw_parts(uniform_buffer &ubo);

private:
	std::vector<part> parts;
};

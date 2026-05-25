#pragma once

#include "../pch.hpp"

class texture {
public:
	texture();
	~texture();
	texture(texture &&t) : tex_id(t.tex_id) {
		t.tex_id = 0;
	}

	void load(const void *data, int width, int height, GLenum format = GL_RGBA, bool srgb = false);
	void load_from_file(const std::filesystem::path &path, bool srgb = true);
	void load_hdr_equirect(
	    const std::filesystem::path &path,
	    glm::vec3 *out_brightest_dir = nullptr
	);
	void bind(uint32_t binding = 0);

private:
	GLuint tex_id;
};

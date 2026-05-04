#include "texture.hpp"

texture::texture() {
	glGenTextures(1, &tex_id);
}

texture::~texture() {
	glDeleteTextures(1, &tex_id);
}

void texture::load(const void *data, int w, int h, GLenum format) {
	glBindTexture(GL_TEXTURE_2D, tex_id);

	GLenum internal_format = (format == GL_RGBA) ? GL_RGBA8 : GL_RGB8;
	glTexImage2D(
	    GL_TEXTURE_2D,
	    0,
	    internal_format,
	    w,
	    h,
	    0,
	    format,
	    GL_UNSIGNED_BYTE,
	    data
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void texture::load_hdr_equirect(const std::filesystem::path &path) {
	stbi_set_flip_vertically_on_load(true);
	int w, h, n;
	float *data = stbi_loadf(path.string().c_str(), &w, &h, &n, 3);
	stbi_set_flip_vertically_on_load(false);

	if (!data) {
		throw std::runtime_error("failed to load HDR: " + path.string());
	}

	glBindTexture(GL_TEXTURE_2D, tex_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	stbi_image_free(data);
}

void texture::bind(uint32_t binding) {
	glActiveTexture(GL_TEXTURE0 + binding);
	glBindTexture(GL_TEXTURE_2D, tex_id);
}

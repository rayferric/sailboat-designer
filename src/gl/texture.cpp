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
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void texture::bind(uint32_t binding) {
	glActiveTexture(GL_TEXTURE0 + binding);
	glBindTexture(GL_TEXTURE_2D, tex_id);
}

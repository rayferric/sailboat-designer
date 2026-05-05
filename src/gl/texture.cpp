#include "texture.hpp"

texture::texture() {
	glGenTextures(1, &tex_id);
}

texture::~texture() {
	glDeleteTextures(1, &tex_id);
}

void texture::load(const void *data, int w, int h, GLenum format, bool srgb) {
	glBindTexture(GL_TEXTURE_2D, tex_id);

	// sRGB internal format makes the GPU decode gamma at sample time, so the
	// shader receives linear data. Required for color textures (glTF baseColor)
	// — without it, PBR math operates on gamma-encoded values and outputs look
	// washed out. Data textures (normal/metallic/roughness) must stay linear.
	GLenum internal_format;
	if (format == GL_RGBA) internal_format = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
	else                   internal_format = srgb ? GL_SRGB8        : GL_RGB8;
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

void texture::load_hdr_equirect(const std::filesystem::path &path, glm::vec3 *out_brightest_dir) {
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

	if (out_brightest_dir) {
		// Scan for brightest pixel — the sun in a clear-sky HDR. Used as the
		// primary directional light so all shaders share the same sun position.
		float max_lum = 0.0f;
		int max_x = 0, max_y = 0;
		const int stride = 4;
		for (int y = 0; y < h; y += stride) {
			for (int x = 0; x < w; x += stride) {
				const float *p = &data[(y * w + x) * 3];
				float lum = 0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
				if (lum > max_lum) { max_lum = lum; max_x = x; max_y = y; }
			}
		}
		// Inverse of the equirect mapping used in the shaders:
		//   uv.x = atan(z,x)/(2π) + 0.5,  uv.y = asin(y)/π + 0.5
		const float PI = 3.14159265f;
		float u = (max_x + 0.5f) / (float)w;
		float v = (max_y + 0.5f) / (float)h;
		float phi   = (u - 0.5f) * 2.0f * PI;
		float theta = (v - 0.5f) * PI;
		*out_brightest_dir = glm::normalize(glm::vec3(
		    std::cos(theta) * std::cos(phi),
		    std::sin(theta),
		    std::cos(theta) * std::sin(phi)
		));
	}

	stbi_image_free(data);
}

void texture::bind(uint32_t binding) {
	glActiveTexture(GL_TEXTURE0 + binding);
	glBindTexture(GL_TEXTURE_2D, tex_id);
}

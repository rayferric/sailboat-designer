#include "framebuffer.hpp"
#include <iostream>

framebuffer::framebuffer(int w, int h) : width(w), height(h), depth_texture(0), fbo_id(0) {
    glGenFramebuffers(1, &fbo_id);
}

void framebuffer::destroy() {
    for (auto tex : color_textures) {
        glDeleteTextures(1, &tex);
    }
    color_textures.clear();
    
    if (depth_texture) {
        glDeleteTextures(1, &depth_texture);
        depth_texture = 0;
    }
    if (fbo_id) {
        glDeleteFramebuffers(1, &fbo_id);
        fbo_id = 0;
    }
}

framebuffer::~framebuffer() {
    destroy();
}

framebuffer::framebuffer(framebuffer&& other) noexcept {
    *this = std::move(other);
}

framebuffer& framebuffer::operator=(framebuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        fbo_id = other.fbo_id;
        color_textures = std::move(other.color_textures);
        color_formats = std::move(other.color_formats);
        depth_texture = other.depth_texture;
        has_depth = other.has_depth;
        width = other.width;
        height = other.height;

        other.fbo_id = 0;
        other.depth_texture = 0;
    }
    return *this;
}

void framebuffer::add_color_attachment(GLenum internal_format, GLenum format, GLenum type) {
    if (fbo_id == 0) glGenFramebuffers(1, &fbo_id);
    color_formats.push_back({internal_format, format, type});

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, 
                 format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, 
                           GL_COLOR_ATTACHMENT0 + color_textures.size(),
                           GL_TEXTURE_2D, tex, 0);
    
    color_textures.push_back(tex);
    update_draw_buffers();
}

void framebuffer::add_depth_attachment() {
    if (fbo_id == 0) glGenFramebuffers(1, &fbo_id);
    has_depth = true;

    glGenTextures(1, &depth_texture);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    
    bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depth_texture, 0);
}

void framebuffer::resize(int w, int h) {
    if (w == width && h == height) return;
    width = w;
    height = h;

    auto old_color_formats = color_formats;
    bool old_has_depth = has_depth;

    destroy();
    color_formats.clear();

    if (width > 0 && height > 0) {
        glGenFramebuffers(1, &fbo_id);
        for (const auto& fmt : old_color_formats) {
            add_color_attachment(std::get<0>(fmt), std::get<1>(fmt), std::get<2>(fmt));
        }
        if (old_has_depth) {
            add_depth_attachment();
        }
    }
}

void framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    glViewport(0, 0, width, height);
}

void framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void framebuffer::bind_color_texture(int texture_unit, int index) const {
    if (index < color_textures.size()) {
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        glBindTexture(GL_TEXTURE_2D, color_textures[index]);
    }
}

void framebuffer::bind_depth_texture(int texture_unit) const {
    if (depth_texture) {
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
    }
}

bool framebuffer::is_complete() const {
    bind();
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void framebuffer::update_draw_buffers() {
    if (color_textures.empty()) {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        return;
    }
    std::vector<GLenum> buffers;
    for (size_t i = 0; i < color_textures.size(); ++i) {
        buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
    }
    glDrawBuffers(buffers.size(), buffers.data());
}

void framebuffer::blit_to(GLuint dest_fbo, int mask, int filter) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest_fbo);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, mask, filter);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
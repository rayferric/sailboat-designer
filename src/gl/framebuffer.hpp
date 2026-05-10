#pragma once

#include "../pch.hpp"

class framebuffer {
private:
    GLuint fbo_id = 0;
    std::vector<GLuint> color_textures;
    std::vector<std::tuple<GLenum, GLenum, GLenum>> color_formats;
    GLuint depth_texture = 0;
    bool has_depth = false;
    int width = 0, height = 0;
    
    void update_draw_buffers();
    void destroy();
    
public:
    framebuffer() = default;
    framebuffer(int w, int h);
    ~framebuffer();

    framebuffer(const framebuffer&) = delete;
    framebuffer& operator=(const framebuffer&) = delete;

    framebuffer(framebuffer&& other) noexcept;
    framebuffer& operator=(framebuffer&& other) noexcept;
    
    void add_color_attachment(GLenum internal_format = GL_RGBA8, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE);
    void add_depth_attachment();
    
    void bind() const;
    static void unbind();
    
    void bind_color_texture(int texture_unit, int index = 0) const;
    void bind_depth_texture(int texture_unit) const;
    
    bool is_complete() const;

    void resize(int w, int h);
    void blit_to(GLuint dest_fbo, int mask = GL_COLOR_BUFFER_BIT, int filter = GL_NEAREST) const;

    GLuint get_fbo() const { return fbo_id; }
};
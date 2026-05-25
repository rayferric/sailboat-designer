#pragma once

#include "pch.hpp"

#include "./gl/shader.hpp"
#include "./gl/texture.hpp"
#include "./gl/uniform_buffer.hpp"
#include "./gl/framebuffer.hpp"
#include "./fps_camera.hpp"
#include "./entity.hpp"

class renderer {
public:
    fps_camera cam;
    float env_yaw = 0.0f;

    renderer();
    
    void draw(const std::shared_ptr<entity>& root);

private:
    shader lit;
    shader water;
    shader sky;
    shader depth;
    shader ssao;
    shader tonemap;
    texture sky_tex;
    glm::vec3 sun_dir;
    uniform_buffer ubo_frame;
    uniform_buffer ubo_entity;
    uniform_buffer ubo_material;

    GLuint empty_vao;

    framebuffer shadow_fb;
    static constexpr int shadow_size = 4096;

    framebuffer opaque_fb;
    framebuffer ssao_fb;
    framebuffer water_fb;

    void draw_recursive(const std::shared_ptr<entity>& current, const glm::mat4& V, const glm::mat4& P, glm::vec4 tint);
};

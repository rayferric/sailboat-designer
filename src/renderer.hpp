#pragma once

#include "pch.hpp"

#include "./gl/shader.hpp"
#include "./gl/texture.hpp"
#include "./gl/uniform_buffer.hpp"
#include "./fps_camera.hpp"
#include "./entity.hpp"

class renderer {
public:
    fps_camera cam;

    renderer();
    
    void draw(const std::shared_ptr<entity>& root);

private:
    shader lit;
    shader water;
    shader sky;
    texture sky_tex;
    uniform_buffer ubo_frame;
    uniform_buffer ubo_entity;
    uniform_buffer ubo_material;

    GLuint empty_vao;

    void draw_recursive(const std::shared_ptr<entity>& current, const glm::mat4& V, const glm::mat4& P, glm::vec4 tint);
};

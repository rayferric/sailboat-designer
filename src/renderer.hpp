#pragma once

#include "pch.hpp"

#include "./gl/shader.hpp"
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
    uniform_buffer ubo_mvp;
    uniform_buffer ubo_mat;

    void draw_recursive(const std::shared_ptr<entity>& current, const glm::mat4& V, const glm::mat4& P);
};

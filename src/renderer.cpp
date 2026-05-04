#include "renderer.hpp"
#include <GLFW/glfw3.h>

renderer::renderer() {
    lit.compile_from_files("assets/lit.vert", "assets/lit.frag");
    water.compile_from_files("assets/water.vert", "assets/water.frag");
    sky.compile_from_files("assets/sky.vert", "assets/sky.frag");

    sky_tex.load_hdr_equirect("assets/sky.hdr");

    glGenVertexArrays(1, &empty_vao);
}

void renderer::draw(const std::shared_ptr<entity>& root) {
    if (!root) return;

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.08f, 0.1f, 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ubo_frame.bind(0);
    ubo_entity.bind(1);
    ubo_material.bind(2);

    glm::mat4 V = cam.calc_view_mat();
    glm::mat4 P = cam.calc_proj_mat();
    ubo_frame.update(V, P, (float)glfwGetTime());

    // Draw sky background
    glDisable(GL_DEPTH_TEST);
    sky.bind();
    sky_tex.bind(0);
    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    lit.bind();
    glm::vec4 tint(0.0f);

    draw_recursive(root, V, P, tint);

    // Draw water (samples HDR sky for reflection and horizon fade)
    water.bind();
    sky_tex.bind(0);
    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 800 * 800 * 6);
    glBindVertexArray(0);
}

void renderer::draw_recursive(const std::shared_ptr<entity>& current, const glm::mat4& V, const glm::mat4& P, glm::vec4 tint) {
    glm::vec4 old_tint = tint;
    if (current->tint) {
        tint = *(current->tint);
    }

    if (current->model_asset) {
        glm::mat4 M = current->get_world_matrix();
        ubo_entity.update(M, tint);
        current->model_asset->draw_parts(ubo_material);
    }

    for (const auto& child : current->children) {
        draw_recursive(child, V, P, tint);
    }

    tint = old_tint;
}

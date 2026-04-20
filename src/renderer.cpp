#include "renderer.hpp"

renderer::renderer() {
    lit.compile_from_files("assets/lit.vert", "assets/lit.frag");
}

void renderer::draw(const std::shared_ptr<entity>& root) {
    if (!root) return;

    ubo_mvp.bind(0);
    ubo_mat.bind(1);
    lit.bind();

    glm::mat4 V = cam.calc_view_mat();
    glm::mat4 P = cam.calc_proj_mat();

    draw_recursive(root, V, P);
}

void renderer::draw_recursive(const std::shared_ptr<entity>& current, const glm::mat4& V, const glm::mat4& P) {
    if (current->model_asset) {
        glm::mat4 M = current->get_world_matrix();
        ubo_mvp.update(M, V, P);
        current->model_asset->draw_parts(ubo_mat);
    }

    for (const auto& child : current->children) {
        draw_recursive(child, V, P);
    }
}

#include "renderer.hpp"
#include <GLFW/glfw3.h>

renderer::renderer() {
    lit.compile_from_files("assets/lit.vert", "assets/lit.frag");
    water.compile_from_files("assets/water.vert", "assets/water.frag");
    sky.compile_from_files("assets/sky.vert", "assets/sky.frag");

    sky_tex.load_hdr_equirect("assets/sky.hdr", &sun_dir);

    glGenVertexArrays(1, &empty_vao);
}

void renderer::draw(const std::shared_ptr<entity>& root) {
    if (!root) return;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.08f, 0.08f, 0.1f, 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ubo_frame.bind(0);
    ubo_entity.bind(1);
    ubo_material.bind(2);

    glm::mat4 V = cam.calc_view_mat();
    glm::mat4 P = cam.calc_proj_mat();
    // Frame UBO std140: mat4 V, mat4 P, float time, [12B pad], vec4 sunDir, vec4 sunColor
    // sunColor is a warm white with magnitude ~10 (in linear HDR units) — handled by tonemap.
    ubo_frame.update(
        V, P, (float)glfwGetTime(),
        glm::vec3(0.0f),
        glm::vec4(sun_dir, 0.0f),
        glm::vec4(10.0f, 9.0f, 8.0f, 1.0f)
    );

    // Draw sky background
    glDisable(GL_DEPTH_TEST);
    sky.bind();
    sky_tex.bind(0);
    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    // Bind sky to unit 1 so lit.frag can sample it for ambient + specular env.
    sky_tex.bind(1);
    lit.bind();
    glm::vec4 tint(0.0f);

    draw_recursive(root, V, P, tint);

    // Draw water (samples HDR sky for reflection and horizon fade).
    // Water mesh is single-sided in winding but visually double-sided — back
    // faces of wave slopes must remain visible, so culling is off here.
    glDisable(GL_CULL_FACE);
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

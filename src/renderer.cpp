#include "renderer.hpp"
#include <GLFW/glfw3.h>

renderer::renderer() {
    lit.compile_from_files("assets/lit.vert", "assets/lit.frag");
    water.compile_from_files("assets/water.vert", "assets/water.frag");
    sky.compile_from_files("assets/sky.vert", "assets/sky.frag");
    depth.compile_from_files("assets/depth.vert", "assets/depth.frag");

    sky_tex.load_hdr_equirect("assets/sky.hdr", &sun_dir);

    glGenVertexArrays(1, &empty_vao);

    // Shadow map: depth-only FBO. Sampled in lit.frag for per-pixel sun shadow.
    glGenTextures(1, &shadow_tex);
    glBindTexture(GL_TEXTURE_2D, shadow_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_size, shadow_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Border = 1.0 means anything outside the light frustum is treated as fully lit.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glGenFramebuffers(1, &shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderer::draw(const std::shared_ptr<entity>& root) {
    if (!root) return;

    ubo_frame.bind(0);
    ubo_entity.bind(1);
    ubo_material.bind(2);

    glm::mat4 V = cam.calc_view_mat();
    glm::mat4 P = cam.calc_proj_mat();

    // Sun's view-projection. Orthographic box centered at origin covers the
    // boat; lighthouse is outside but doesn't need to cast shadows. Placed far
    // enough back along sun_dir to fit the full vertical extent of the scene.
    glm::vec3 lightPos = sun_dir * 30.0f;
    glm::mat4 lightV = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightP = glm::ortho(-8.0f, 8.0f, -8.0f, 8.0f, 0.1f, 60.0f);
    glm::mat4 lightVP = lightP * lightV;

    // Frame UBO std140: mat4 V, mat4 P, float time, [12B pad], vec4 sunDir, vec4 sunColor, mat4 lightVP
    // sunColor is a warm white with magnitude ~10 (in linear HDR units) — handled by tonemap.
    ubo_frame.update(
        V, P, (float)glfwGetTime(),
        glm::vec3(0.0f),
        glm::vec4(sun_dir, 0.0f),
        glm::vec4(10.0f, 9.0f, 8.0f, 1.0f),
        lightVP
    );

    // --- Shadow pass: render scene depth from sun's POV. ---
    GLint prev_viewport[4];
    glGetIntegerv(GL_VIEWPORT, prev_viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glViewport(0, 0, shadow_size, shadow_size);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
    glClear(GL_DEPTH_BUFFER_BIT);
    depth.bind();
    {
        glm::vec4 tint(0.0f);
        draw_recursive(root, lightV, lightP, tint);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);

    // --- Main pass ---
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);
    glClearColor(0.08f, 0.08f, 0.1f, 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw sky background
    glDisable(GL_DEPTH_TEST);
    sky.bind();
    sky_tex.bind(0);
    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    // Bind sky to unit 1 (env IBL) and shadow map to unit 2.
    sky_tex.bind(1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, shadow_tex);
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

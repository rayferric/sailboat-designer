#include "renderer.hpp"
#include <GLFW/glfw3.h>

renderer::renderer() {
    lit.compile_from_files("assets/shaders/lit.vert", "assets/shaders/lit.frag");
    water.compile_from_files("assets/shaders/water.vert", "assets/shaders/water.frag");
    sky.compile_from_files("assets/shaders/sky.vert", "assets/shaders/sky.frag");
    depth.compile_from_files("assets/shaders/depth.vert", "assets/shaders/depth.frag");
    tonemap.compile_from_files("assets/shaders/tonemap.vert", "assets/shaders/tonemap.frag");

    sky_tex.load_hdr_equirect("assets/sky.hdr", &sun_dir);

    glGenVertexArrays(1, &empty_vao);

    // Shadow map: depth-only FBO. Sampled in lit.frag for per-pixel sun shadow.
    shadow_fb = framebuffer(shadow_size, shadow_size);
    shadow_fb.add_depth_attachment();
    
    opaque_fb = framebuffer(1, 1);
    opaque_fb.add_color_attachment(GL_RGBA16F, GL_RGBA, GL_FLOAT);
    opaque_fb.add_depth_attachment();
    
    water_fb = framebuffer(1, 1);
    water_fb.add_color_attachment(GL_RGBA16F, GL_RGBA, GL_FLOAT);
    water_fb.add_depth_attachment();
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
        glm::vec4(10.0f, 9.0f, 8.0f, 1.0f) * 0.5f,
        lightVP
    );

    // --- Shadow pass: render scene depth from sun's POV. ---
    GLint prev_viewport[4];
    glGetIntegerv(GL_VIEWPORT, prev_viewport);

    shadow_fb.bind();
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
    
    // --- Setup Main HDR FBOs ---
    int w = prev_viewport[2];
    int h = prev_viewport[3];
    opaque_fb.resize(w, h);
    water_fb.resize(w, h);

    // --- Main Opaque Pass (to opaque_fb) ---
    opaque_fb.bind();
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
    shadow_fb.bind_depth_texture(2);
    lit.bind();
    glm::vec4 tint(0.0f);

    draw_recursive(root, V, P, tint);

    // --- Prepare Aux FBO for Water Pass ---
    opaque_fb.blit_to(water_fb.get_fbo(), GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    water_fb.bind(); // Need to draw water into water_fb so water shader can sample opaque_fb while overwriting pixels in water_fb.

    // Draw water (samples HDR sky for reflection and opaque tex for refraction).
    // Water mesh is single-sided in winding but visually double-sided — back
    // faces of wave slopes must remain visible, so culling is off here.
    glDisable(GL_CULL_FACE);
    
    // Disable blend, as water frag overwrites alpha directly and does own mix
    glDisable(GL_BLEND);

    water.bind();
    sky_tex.bind(0);
    opaque_fb.bind_color_texture(1);
    opaque_fb.bind_depth_texture(2);

    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 800 * 800 * 6);
    glBindVertexArray(0);
    
    // --- Tonemap Pass (to Default FBO) ---
    framebuffer::unbind();
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    
    tonemap.bind();
    water_fb.bind_color_texture(0);

    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
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

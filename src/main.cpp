#include "./pch.hpp"

#include "./gl/model.hpp"
#include "./gl/shader.hpp"
#include "./gl/uniform_buffer.hpp"
#include "./gl/window.hpp"

#include "./fps_camera.hpp"
#include "./imgui.hpp"

int main() {
	window window;
	window.open(800, 600, "Sailboat Designer");

	imgui ui(window.glfw_window);

	glEnable(GL_DEPTH_TEST);
	glClearColor(0.08, 0.08, 0.1, 0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	model sailboat;
	sailboat.load_from_glb(
	    "/home/rayferric/School/gis1-pg3d/sailboat-designer/assets/sailboat.glb"
	);
	shader lit;
	lit.compile_from_files(
	    "/home/rayferric/School/gis1-pg3d/sailboat-designer/assets/lit.vert",
	    "/home/rayferric/School/gis1-pg3d/sailboat-designer/assets/lit.frag"
	);
	fps_camera cam;
	cam.pos.z = 20.0f; // Move camera back so we can see the model
	uniform_buffer ubo;

	// clang-format off
	window.run_loop({
		.on_update = [&](float dt) {
			ui.begin();
			ImGui::Begin("My Window");
			ImGui::Text("Hello world");
			if (ImGui::Button("Click me")) {
				// ...
			}
			ImGui::End();
			ui.end();

			if (!ui.is_cursor_hovering_over() || cam.is_cursor_captured()) {
				cam.update_fps_pose_from_glfw_input(window.glfw_window, dt);
			}

			glm::mat4 M = glm::mat4(1.0f);
			glm::mat4 V = cam.calc_view_mat();
			glm::mat4 P = cam.calc_proj_mat();
			ubo.update(M, V, P);
		},
		.on_draw = [&]() {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			ubo.bind(0);
			lit.bind();
			sailboat.draw();

			ui.draw();
		},
		.on_resize = [&](uint32_t w, uint32_t h) {
			glViewport(0, 0, w, h);
			cam.width = w;
			cam.height = h;
		}
	});
	// clang-format on

	return 0;
}

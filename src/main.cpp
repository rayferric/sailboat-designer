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

	model boat_base, boat_sail;
	boat_base.load_from_glb("assets/kuba_boat_base.glb");
	boat_sail.load_from_glb("assets/kuba_boat_sail.glb");
	shader lit;
	lit.compile_from_files("assets/lit.vert", "assets/lit.frag");
	uniform_buffer ubo_mvp;
	uniform_buffer ubo_mat;

	fps_camera cam;
	cam.pos.x = 10.0f;
	cam.pos.y = 6.0f;
	cam.pos.z = 10.0f;
	cam.pitch = -10.0f;
	cam.yaw = 55.0f;

	float sail_angle = 0.0f;

	// clang-format off
	window.run_loop({
		.on_update = [&](float dt) {
			ui.begin();
			ImGui::SetNextWindowSize(ImVec2(300, 130), ImGuiCond_FirstUseEver);
			ImGui::Begin("Controls");
			ImGui::TextWrapped(
				"FPS Camera:\n"
				"  LMB in viewport: Start flying\n"
				"  WSADEQ: Movement commands\n"
				"  SHIFT/CTRL: Movement speed\n"
				"  ESC: Show cursor"
			);
			ImGui::Separator();
			ImGui::SliderFloat("Sail Angle", &sail_angle, -90.0f, 90.0f, "%.1f deg");
			ImGui::End();
			ui.end();

			if (!ui.is_cursor_hovering_over() || cam.is_cursor_captured()) {
				cam.update_fps_pose_from_glfw_input(window.glfw_window, dt);
			}
		},
		.on_draw = [&]() {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			ubo_mvp.bind(0);
			ubo_mat.bind(1);
			lit.bind();

			glm::mat4 M_base = glm::mat4(1.0f);
			glm::mat4 V = cam.calc_view_mat();
			glm::mat4 P = cam.calc_proj_mat();
			ubo_mvp.update(M_base, V, P);
			
			// Draw base
			boat_base.draw_parts(ubo_mat);
			
			// Draw sail with rotation
			glm::mat4 M_sail = glm::rotate(glm::mat4(1.0f), 
			                               glm::radians(sail_angle), 
			                               glm::vec3(0.0f, 1.0f, 0.0f));
			ubo_mvp.update(M_sail, V, P);
			boat_sail.draw_parts(ubo_mat);

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
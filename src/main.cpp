#include "./pch.hpp"

#include "./gl/window.hpp"
#include "./imgui.hpp"

#include "./entity.hpp"
#include "./renderer.hpp"
#include "./raycaster.hpp"

std::shared_ptr<entity> load_prop(const std::filesystem::path& path) {
	auto ent = std::make_shared<entity>();
	ent->model_asset = std::make_shared<model>();
	ent->model_asset->load_from_glb(path);
	ent->collider_asset = std::make_shared<collider>();
	ent->collider_asset->load_from_glb(path);
	return ent;
}

int main() {
	window window;
	window.open(800, 600, "Sailboat Designer");

	imgui ui(window.glfw_window);

	glEnable(GL_DEPTH_TEST);
	glClearColor(0.08, 0.08, 0.1, 0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	renderer renderer;
	renderer.cam.pos.x = 10.0f;
	renderer.cam.pos.y = 6.0f;
	renderer.cam.pos.z = 10.0f;
	renderer.cam.pitch = -10.0f;
	renderer.cam.yaw = 55.0f;

	auto root = std::make_shared<entity>();

	auto boat_base = load_prop("assets/kuba_boat_base.glb");
	auto boat_sail = load_prop("assets/kuba_boat_sail.glb");
	auto test_cube = load_prop("assets/test_cube.glb");

	root->add_child(boat_base);
	boat_base->add_child(boat_sail);
	
	// Attach test cube directly to root, so we don't raycast against it when checking boat_base
	root->add_child(test_cube);

	raycaster rc;

	float sail_angle = 0.0f;
	bool debug_cube_enabled = false;

	// clang-format off
	window.run_loop({
		.on_update = [&](float dt) {
			ui.begin();
			ImGui::SetNextWindowSize(ImVec2(300, 160), ImGuiCond_FirstUseEver);
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
			ImGui::Separator();
			ImGui::Checkbox("Enable Raycast Debug Cube", &debug_cube_enabled);
			ImGui::End();
			ui.end();

			if (!debug_cube_enabled) {
				// Normal FPS camera mode
				if (!ui.is_cursor_hovering_over() || renderer.cam.is_cursor_captured()) {
					renderer.cam.update_fps_pose_from_glfw_input(window.glfw_window, dt);
				}
				// Hide the cube far out of bounds when disabled
				test_cube->position = glm::vec3(0.0f, -10000.0f, 0.0f);
			} else {
				// Debug cube place mode - skip camera's glfw_input poll so left click won't capture cursor
				
				// Place the test cube under the cursor
				if (!ui.is_cursor_hovering_over()) {
					ray r = rc.gen_mouse_cursor_ray(window, renderer.cam);
					hit_result hit = rc.cast_ray(boat_base, r); // Raycast only against boat hierarchy

					if (hit.has_hit()) {
						test_cube->position = hit.point;
						
						glm::vec3 y_axis = hit.normal;
						glm::vec3 fallback = glm::vec3(1.0f, 0.0f, 0.0f);
						if (std::abs(glm::dot(y_axis, fallback)) > 0.999f) {
							fallback = glm::vec3(0.0f, 1.0f, 0.0f);
						}
						
						glm::vec3 z_axis = glm::normalize(glm::cross(fallback, y_axis));
						glm::vec3 x_axis = glm::normalize(glm::cross(y_axis, z_axis));
						
						test_cube->rotation = glm::quat_cast(glm::mat3(x_axis, y_axis, z_axis));
					} else {
						// Hide the cube if aiming at the sky
						test_cube->position = glm::vec3(0.0f, -10000.0f, 0.0f);
					}
				}
			}
		},
		.on_draw = [&]() {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			// Apply rotation to the sail locally
			boat_sail->rotation = glm::angleAxis(glm::radians(sail_angle), glm::vec3(0.0f, 1.0f, 0.0f));

			renderer.draw(root);

			ui.draw();
		},
		.on_resize = [&](uint32_t w, uint32_t h) {
			glViewport(0, 0, w, h);
			renderer.cam.width = w;
			renderer.cam.height = h;
		}
	});
	// clang-format on

	return 0;
}
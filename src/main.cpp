#include "./gl/window.hpp"
#include "./imgui.hpp"

#include "./entity.hpp"
#include "./renderer.hpp"
#include "./raycaster.hpp"
#include "./prop_editor.hpp"
#include "./imgui.h"

// enable discrete GPU on laptops with both integrated and dedicated graphics
extern "C" {
	_declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

std::shared_ptr<entity> load_prop(const std::filesystem::path& path) {
    auto ent = std::make_shared<entity>();
    ent->model_asset = std::make_shared<model>();
    ent->model_asset->load_from_glb(path);
    ent->collider_asset = std::make_shared<collider>();
    ent->collider_asset->load_from_glb(path);
    return ent;
}

struct boat_model_def {
    std::string name;
    std::string base_path;
    std::string sail_path;
};

int main() {
  try {
    window window;
    window.open(1280, 720, "Sailboat Designer - Props Editor");

    imgui ui(window.glfw_window);

    renderer renderer;
    renderer.cam.pos.x = 10.0f;
    renderer.cam.pos.y = 6.0f;
    renderer.cam.pos.z = 10.0f;
    renderer.cam.pitch = -10.0f;
    renderer.cam.yaw = 55.0f;

    raycaster raycaster;

    // --- Boat Models ---
    std::vector<boat_model_def> boat_models = {
        {"Long Hull", "assets/models/kuba_boat_base.glb", "assets/models/kuba_boat_sail.glb"},
        {"Short Hull", "assets/models/kuba_boat_2_base.glb", "assets/models/kuba_boat_2_sail.glb"}
    };
    int selected_boat_index = 0;

    // --- Scene ---
    auto root = std::make_shared<entity>();
    auto boat_base = load_prop(boat_models[selected_boat_index].base_path);
    auto boat_sail = load_prop(boat_models[selected_boat_index].sail_path);
    auto lighthouse = load_prop("assets/models/lighthouse.glb");
    lighthouse->position = glm::vec3(-20.0f, 0.0f, -50.0f);
    lighthouse->rotation = glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    root->add_child(boat_base);
    root->add_child(lighthouse);
    boat_base->add_child(boat_sail); // Attach sail to boat_base

    // --- Props ---
    prop_editor props;
    props.register_prop_type("Test Cube", "assets/models/test_cube.glb");
    props.register_prop_type("Lifebuoy", "assets/models/lifebuoy.glb");
    props.register_prop_type("Ducker", "assets/models/ducker.glb");

    float sail_angle = 0.0f;
    float boat_height = 0.0f;
    glm::vec3 primary_color = glm::vec3(38, 49, 53) / 255.0f;
    glm::vec3 secondary_color = glm::vec3(1.0f);
    glm::vec3 emblem_color = glm::vec3(14, 20, 58) / 255.0f;
    std::string emblem_path = "assets/default_emblem.png";

    // Load colors and emblem into boat materials
    boat_base->model_asset->set_material_texture("SBD_FLAG_ICON", emblem_path);
    boat_base->model_asset->set_material_color("SBD_COLOR_PRIMARY", glm::vec4(primary_color, 1.0f));
    boat_base->model_asset->set_material_color("SBD_COLOR_SECONDARY", glm::vec4(secondary_color, 1.0f));
    boat_base->model_asset->set_material_color("SBD_FLAG_COLOR", glm::vec4(emblem_color, 1.0f));

    // --- Main Loop ---
    // clang-format off
    window.run_loop({
        .on_update = [&](float dt) {
            ui.begin();
			bool ui_disabled = renderer.cam.is_cursor_captured();
			if (ui_disabled) {
                ImGui::BeginDisabled();
            }
            
			// Editor and camera update
            bool editor_in_use = props.update(window, renderer.cam, raycaster, root, root, ui.is_cursor_hovering_over());
			bool prevent_mouse_capture = editor_in_use || ui.is_cursor_hovering_over();
			renderer.cam.update_fps_pose_from_glfw_input(window.glfw_window, dt, prevent_mouse_capture);

            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
            ImGui::Begin("Boat Params");
            ImGui::TextWrapped(
                "FPS Camera:\n"
                "  LMB in viewport: Start flying\n"
                "  ESC: Show cursor"
            );
            ImGui::Separator();

			if (ImGui::BeginCombo("Model", boat_models[selected_boat_index].name.c_str())) {
				for (int n = 0; n < boat_models.size(); n++) {
					const bool is_selected = (selected_boat_index == n);
					if (ImGui::Selectable(boat_models[n].name.c_str(), is_selected)) {
						if (selected_boat_index != n) {
                            selected_boat_index = n;
                            
                            // Remove old boat
                            root->remove_child(boat_base);
                            
                            // Remove all props
                            props.clear_all_props();

                            // Load new boat
                            boat_base = load_prop(boat_models[selected_boat_index].base_path);
                            boat_sail = load_prop(boat_models[selected_boat_index].sail_path);
                            
                            root->add_child(boat_base);
                            boat_base->add_child(boat_sail);
                            
                            // Keep boat transform relative to root or reset it
                            boat_base->position = glm::vec3(0.0f, boat_height, 0.0f);
                            boat_sail->rotation = glm::angleAxis(glm::radians(sail_angle), glm::vec3(0.0f, 1.0f, 0.0f));

                            // Restore colors to new boat material
                            boat_base->model_asset->set_material_color("SBD_COLOR_PRIMARY", glm::vec4(primary_color, 1.0f));
                            boat_base->model_asset->set_material_color("SBD_COLOR_SECONDARY", glm::vec4(secondary_color, 1.0f));
                            boat_base->model_asset->set_material_color("SBD_FLAG_COLOR", glm::vec4(emblem_color, 1.0f));
                            boat_base->model_asset->set_material_texture("SBD_FLAG_ICON", emblem_path);
						}
					}
					if (is_selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

            ImGui::SliderFloat("Sail Angle", &sail_angle, -90.0f, 90.0f, "%.1f deg");
            ImGui::SliderFloat("Boat Height", &boat_height, -2.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Env Yaw", &renderer.env_yaw, 0.0f, 360.0f, "%.1f deg");
			
			ImGui::Separator();
			if (ImGui::ColorEdit3("Primary Color", &primary_color[0])) {
				boat_base->model_asset->set_material_color("SBD_COLOR_PRIMARY", glm::vec4(primary_color, 1.0f));
			}
			if (ImGui::ColorEdit3("Secondary Color", &secondary_color[0])) {
				boat_base->model_asset->set_material_color("SBD_COLOR_SECONDARY", glm::vec4(secondary_color, 1.0f));
			}			if (ImGui::ColorEdit3("Emblem Color", &emblem_color[0])) {
				boat_base->model_asset->set_material_color("SBD_FLAG_COLOR", glm::vec4(emblem_color, 1.0f));
			}			if (ImGui::Button("Select Emblem (PNG/JPG)")) {
				auto selection = pfd::open_file("Select Emblem", ".",
					{ "Image Files", "*.png *.jpg *.jpeg" }).result();
				if (!selection.empty()) {
                    emblem_path = selection[0];
					boat_base->model_asset->set_material_texture("SBD_FLAG_ICON", emblem_path);
				}
			}
            ImGui::End();
			
            if (ui_disabled) {
                ImGui::EndDisabled();
            }
            ui.end();

			boat_sail->rotation = glm::angleAxis(glm::radians(sail_angle), glm::vec3(0.0f, 1.0f, 0.0f));
            boat_base->position = glm::vec3(0.0f, boat_height, 0.0f);
        },
        .on_draw = [&]() {
            renderer.draw(root);
            ui.draw();
        },
        .on_resize = [&](uint32_t w, uint32_t h) {
            if (w == 0 || h == 0) return;
            glViewport(0, 0, w, h);
            renderer.cam.width = w;
            renderer.cam.height = h;
        }
    });
    // clang-format on

    return 0;
  } catch (const std::exception &e) {
    std::cout << "Fatal error: " << e.what() << "\n";
    return 1;
  }
}
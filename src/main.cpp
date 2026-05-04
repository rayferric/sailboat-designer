#include "./gl/window.hpp"
#include "./imgui.hpp"

#include "./entity.hpp"
#include "./renderer.hpp"
#include "./raycaster.hpp"
#include "./prop_editor.hpp"
#include "./imgui.h"
#include "GLFW/glfw3.h"
#include <windows.h>

std::shared_ptr<entity> load_prop(const std::filesystem::path& path) {
    auto ent = std::make_shared<entity>();
    ent->model_asset = std::make_shared<model>();
    ent->model_asset->load_from_glb(path);
    ent->collider_asset = std::make_shared<collider>();
    ent->collider_asset->load_from_glb(path);
    return ent;
}

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

    // --- Scene ---
    auto root = std::make_shared<entity>();
    auto boat_base = load_prop("assets/kuba_boat_base.glb");
    auto boat_sail = load_prop("assets/kuba_boat_sail.glb");
    auto lighthouse = load_prop("assets/lighthouse.glb");
    lighthouse->position = glm::vec3(-20.0f, 0.0f, -50.0f);
    lighthouse->rotation = glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    root->add_child(boat_base);
    root->add_child(lighthouse);
    boat_base->add_child(boat_sail); // Attach sail to boat_base

    // --- Props ---
    prop_editor props;
    props.register_prop_type("Test Cube", "assets/test_cube.glb");
    props.register_prop_type("Lifebuoy", "assets/lifebuoy.glb");

    float sail_angle = 0.0f;
    float boat_height = 0.0f;

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
            ImGui::SetNextWindowSize(ImVec2(300, 110), ImGuiCond_FirstUseEver);
            ImGui::Begin("Boat Params");
            ImGui::TextWrapped(
                "FPS Camera:\n"
                "  LMB in viewport: Start flying\n"
                "  ESC: Show cursor"
            );
            ImGui::Separator();
            ImGui::SliderFloat("Sail Angle", &sail_angle, -90.0f, 90.0f, "%.1f deg");
            ImGui::SliderFloat("Boat Height", &boat_height, -2.0f, 2.0f, "%.2f");
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
            glViewport(0, 0, w, h);
            renderer.cam.width = w;
            renderer.cam.height = h;
        }
    });
    // clang-format on

    return 0;
  } catch (const std::exception &e) {
    MessageBoxA(nullptr, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);
    return 1;
  }
}
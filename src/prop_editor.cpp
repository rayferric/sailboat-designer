#include "prop_editor.hpp"
#include "imgui.h"
#include <algorithm>

void prop_editor::register_prop_type(const std::string& name, std::shared_ptr<model> m, std::shared_ptr<collider> c) {
    registered_props.emplace_back(name, m, c);
}

void prop_editor::register_prop_type(const std::string& name, const std::filesystem::path& path) {
    auto model_asset = std::make_shared<model>();
    auto collider_asset = std::make_shared<collider>();
    
    model_asset->load_from_glb(path);
    collider_asset->load_from_glb(path);

    register_prop_type(name, model_asset, collider_asset);
}

void prop_editor::abort_current_operation(const std::shared_ptr<entity>& root) {
    if (current_mode == mode::placing && active_placement_prop) {
        root->remove_child(active_placement_prop);
        active_placement_prop = nullptr;
    }
    if (current_mode == mode::removing && hovered_prop) {
        hovered_prop->tint = nullptr;
        hovered_prop = nullptr;
    }
    current_mode = mode::inactive;
}

bool prop_editor::update(const window& win, const fps_camera& cam, const raycaster& rc, 
                         const std::shared_ptr<entity>& root, const std::shared_ptr<entity>& target_hierarchy, 
                         bool ui_hovered) 
{
    bool lmb_released = glfwGetMouseButton(win.glfw_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE;
    lmb_just_released = lmb_released && !last_lmb;
    
    bool esc_pressed = glfwGetKey(win.glfw_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    bool rmb_pressed = glfwGetMouseButton(win.glfw_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool abort_signal = esc_pressed || rmb_pressed;

    if (abort_signal) {
        abort_current_operation(root);
    }

    renderGUI(root);

    switch(current_mode) {
        case mode::placing:
            handleModePlacing(win, cam, rc, root, target_hierarchy, ui_hovered);
            break;
        case mode::removing:
            handleModeRemoving(win, cam, rc, root, target_hierarchy, ui_hovered);
            break;
    }

    last_lmb = lmb_released;
    return current_mode != mode::inactive;
}

void prop_editor::renderGUI(const std::shared_ptr<entity>& root) {
    ImGui::SetNextWindowPos(ImVec2(0, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Prop Tools");

    ImGui::Text("Current Tool: %s", 
        current_mode == mode::inactive ? "None" : 
        current_mode == mode::placing ? "Placing Prop" : "Removing Props"
    );
    ImGui::Separator();

    for (const auto& def : registered_props) {
        if (ImGui::Button(("Place " + def.name).c_str(), ImVec2(-1, 30))) {
            abort_current_operation(root);
            current_mode = mode::placing;
            
            active_placement_prop = std::make_shared<entity>();
            active_placement_prop->model_asset = def.model_asset;
            active_placement_prop->collider_asset = def.collider_asset;
            root->add_child(active_placement_prop);
        }
    }

    if (ImGui::Button("Remove Props", ImVec2(-1, 30))) {
        abort_current_operation(root);
        current_mode = mode::removing;
    }

    if (current_mode != mode::inactive) {
        if (ImGui::Button("Stop Tool (ESC / RMB)", ImVec2(-1, 30))) {
            abort_current_operation(root);
        }
    }

    ImGui::End();
}

void prop_editor::handleModePlacing(const window& win, const fps_camera& cam, const raycaster& rc, 
                         const std::shared_ptr<entity>& root, const std::shared_ptr<entity>& target_hierarchy, 
                         bool ui_hovered) {
    active_placement_prop->tint = std::make_shared<glm::vec4>(0.0f, 1.0f, 0.0f, 0.5f);; // Green ghost
        
    if(ui_hovered) return;

    ray r = rc.gen_mouse_cursor_ray(win, cam);
    hit_result hit = rc.cast_ray(target_hierarchy, r, active_placement_prop.get());
    
    if (!hit.has_hit()) {
        active_placement_prop->position = glm::vec3(0.0f, -10000.0f, 0.0f);
        return;
    }
    active_placement_prop->position = hit.point;
    
    glm::vec3 y_axis = hit.normal;
    glm::vec3 fallback = glm::vec3(1.0f, 0.0f, 0.0f);
    if (std::abs(glm::dot(y_axis, fallback)) > 0.999f) fallback = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 z_axis = glm::normalize(glm::cross(fallback, y_axis));
    glm::vec3 x_axis = glm::normalize(glm::cross(y_axis, z_axis));
    active_placement_prop->rotation = glm::quat_cast(glm::mat3(x_axis, y_axis, z_axis));
    
    if(!lmb_just_released) return;

    auto hit_ent = hit.hit_entity;
    if (!hit_ent) return;

    glm::mat4 local_mat = glm::inverse(hit_ent->get_world_matrix()) * active_placement_prop->get_world_matrix();
    active_placement_prop->position = local_mat[3];
    active_placement_prop->rotation = glm::quat_cast(local_mat);
    active_placement_prop->tint = nullptr;
    
    root->remove_child(active_placement_prop);
    hit_ent->add_child(active_placement_prop);
    placed_props.push_back(active_placement_prop);
    
    active_placement_prop = nullptr;
    current_mode = mode::inactive;
}

void prop_editor::handleModeRemoving(const window& win, const fps_camera& cam, const raycaster& rc, 
                         const std::shared_ptr<entity>& root, const std::shared_ptr<entity>& target_hierarchy, 
                         bool ui_hovered) {
    if (hovered_prop) {
        hovered_prop->tint = nullptr;
        hovered_prop = nullptr;
    }
    
    if (ui_hovered) return;
    ray r = rc.gen_mouse_cursor_ray(win, cam);
    hit_result hit = rc.cast_ray(target_hierarchy, r);
    
    if(!hit.has_hit()) return;
    auto it = std::find(placed_props.begin(), placed_props.end(), hit.hit_entity);
    
    if(it == placed_props.end()) return;
    hovered_prop = hit.hit_entity;
    hovered_prop->tint = std::make_shared<glm::vec4>(1.0f, 0.0f, 0.0f, 0.5f);
    
    if(!lmb_just_released) return;
    if (auto p = hovered_prop->parent.lock()) {
        p->remove_child(hovered_prop);
    }
    placed_props.erase(it);
    hovered_prop = nullptr;
}
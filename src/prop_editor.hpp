#pragma once

#include "pch.hpp"

#include "entity.hpp"
#include "raycaster.hpp"
#include "fps_camera.hpp"
#include "./gl/window.hpp"

struct prop_definition {
    std::string name;
    std::shared_ptr<model> model_asset;
    std::shared_ptr<collider> collider_asset;
};

class prop_editor {
public:
    void register_prop_type(const std::string& name, std::shared_ptr<model> m, std::shared_ptr<collider> c);

    void register_prop_type(const std::string& name, const std::filesystem::path& path);

    // Draws the ImGui tools and processes input mode logic.
    // Returns 'true' if the user is actively placing or removing a prop,
    // which signals to main.cpp that the camera should not capture the mouse.
    bool update(const window& win, const fps_camera& cam, const raycaster& rc, 
                const std::shared_ptr<entity>& root, const std::shared_ptr<entity>& target_hierarchy, 
                bool ui_hovered);

private:
    enum class mode {
        inactive,
        placing,
        removing
    };

    void renderGUI(const std::shared_ptr<entity>& root);
    void handleModePlacing(const window& win, const fps_camera& cam, const raycaster& rc, 
                         const std::shared_ptr<entity>& root, const std::shared_ptr<entity>& target_hierarchy, 
                         bool ui_hovered);
    void handleModeRemoving(const window& win, const fps_camera& cam, const raycaster& rc, 
                         const std::shared_ptr<entity>& root, const std::shared_ptr<entity>& target_hierarchy, 
                         bool ui_hovered);

    mode current_mode = mode::inactive;
    bool last_lmb = false;
    bool lmb_just_released = false;

    std::vector<prop_definition> registered_props;
    std::vector<std::shared_ptr<entity>> placed_props;

    std::shared_ptr<entity> active_placement_prop = nullptr;
    std::shared_ptr<entity> hovered_prop = nullptr;

    void abort_current_operation(const std::shared_ptr<entity>& root);
};
#pragma once

#include "pch.hpp"

#include "./gl/model.hpp"
#include "./collider.hpp"

class entity : public std::enable_shared_from_this<entity> {
public:
    std::weak_ptr<entity> parent;
    std::vector<std::shared_ptr<entity>> children;

    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

    // components
    std::shared_ptr<model> model_asset;
    std::shared_ptr<collider> collider_asset;

    void add_child(std::shared_ptr<entity> child);

    glm::mat4 get_local_matrix() const;
    glm::mat4 get_world_matrix() const;
};

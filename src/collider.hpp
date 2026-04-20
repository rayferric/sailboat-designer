#pragma once

#include "pch.hpp"

struct collider {
    glm::vec3 aabb_min{std::numeric_limits<float>::max()};
    glm::vec3 aabb_max{std::numeric_limits<float>::lowest()};
    std::vector<glm::vec3> triangles;

    void load_from_glb(const std::filesystem::path& path);
};

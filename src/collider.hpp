#pragma once

#include "pch.hpp"

struct bvh_node {
    glm::vec3 aabb_min;
    uint32_t left_idx_or_tri_begin;
    glm::vec3 aabb_max;
    uint32_t right_idx_or_tri_count;
};

struct collider {
    glm::vec3 aabb_min{std::numeric_limits<float>::max()};
    glm::vec3 aabb_max{std::numeric_limits<float>::lowest()};
    std::vector<glm::vec3> triangles;
    std::vector<bvh_node> bvh_nodes;

    void load_from_glb(const std::filesystem::path& path);
    void build_bvh();
};

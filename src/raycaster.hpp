#pragma once

#include "pch.hpp"

#include "./entity.hpp"
#include "./fps_camera.hpp"
#include "./gl/window.hpp"

struct ray {
    glm::vec3 origin;
    glm::vec3 dir;
};

struct hit_result {
    std::shared_ptr<entity> hit_entity = nullptr;
    
    float distance = -1.0f;
    glm::vec3 point{0.0f};  // World-space intersection point
    glm::vec3 normal{0.0f}; // The surface normal
    
    bool has_hit() const { return hit_entity != nullptr; }
};

class raycaster {
public:
    raycaster() = default;

    ray gen_mouse_cursor_ray(const window& win, const fps_camera& cam) const;
    hit_result cast_ray(const std::shared_ptr<entity>& root_node, const ray& world_ray) const;

private:
    void cast_recursive(const std::shared_ptr<entity>& current, const ray& world_ray, hit_result& closest_hit) const;
    bool intersect_aabb(const glm::vec3& aabb_min, const glm::vec3& aabb_max, const ray& local_ray) const;
    hit_result intersect_triangle(const ray& r, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) const;
};

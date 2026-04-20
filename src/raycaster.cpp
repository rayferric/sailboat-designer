#include "raycaster.hpp"

ray raycaster::gen_mouse_cursor_ray(const window& win, const fps_camera& cam) const {
    double mouse_x, mouse_y;
    glfwGetCursorPos(win.glfw_window, &mouse_x, &mouse_y);

    int w, h;
    glfwGetFramebufferSize(win.glfw_window, &w, &h);
    
    // Safety check just in case window minimizes
    if (w <= 0 || h <= 0) return {cam.pos, glm::vec3(0, 0, -1)};

    // Map to Normalized Device Coordinates (NDC) [-1, 1]
    float ndc_x = (2.0f * static_cast<float>(mouse_x)) / w - 1.0f;
    float ndc_y = 1.0f - (2.0f * static_cast<float>(mouse_y)) / h; // Invert Y

    glm::vec4 clip_coords(ndc_x, ndc_y, -1.0f, 1.0f);
    
    glm::mat4 proj = cam.calc_proj_mat();
    glm::mat4 view = cam.calc_view_mat();

    // To Eye Coordinates
    glm::vec4 eye_coords = glm::inverse(proj) * clip_coords;
    eye_coords = glm::vec4(eye_coords.x, eye_coords.y, -1.0f, 0.0f);

    // To World Coordinates
    glm::vec3 world_dir = glm::vec3(glm::inverse(view) * eye_coords);
    world_dir = glm::normalize(world_dir);

    return { cam.pos, world_dir };
}

hit_result raycaster::intersect_triangle(const ray& r, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) const {
    glm::mat3 m(a - b, a - c, r.dir);
    
    float det = glm::determinant(m);
    if (std::abs(det) < 1e-6f) return hit_result{};

    glm::vec3 v = a - r.origin;
    glm::vec3 w = glm::inverse(m) * v; // [beta, gamma, t]

    float beta = w.x;
    float gamma = w.y;
    float t = w.z;
    float alpha = 1.0f - beta - gamma;

    if (alpha < 0.0f || beta < 0.0f || gamma < 0.0f || t < 0.0f) {
        return hit_result{};
    }

    hit_result res;
    res.distance = t;
    res.point = r.origin + r.dir * t;
    res.normal = glm::normalize(glm::cross(b - a, c - a));
    return res;
}

bool raycaster::intersect_aabb(const glm::vec3& aabb_min, const glm::vec3& aabb_max, const ray& r) const {
    glm::vec3 inv_dir = 1.0f / r.dir;
    glm::vec3 t0 = (aabb_min - r.origin) * inv_dir;
    glm::vec3 t1 = (aabb_max - r.origin) * inv_dir;

    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);

    float tnear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float tfar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

    return tfar >= tnear && tfar > 0.0f;
}

hit_result raycaster::cast_ray(const std::shared_ptr<entity>& root_node, const ray& world_ray) const {
    hit_result closest;
    cast_recursive(root_node, world_ray, closest);
    return closest;
}

void raycaster::cast_recursive(const std::shared_ptr<entity>& current, const ray& world_ray, hit_result& closest_hit) const {
    if (!current) return;

    if (current->collider_asset && !current->collider_asset->triangles.empty()) {
        glm::mat4 inv_world = glm::inverse(current->get_world_matrix());
        
        ray local_ray;
        local_ray.origin = glm::vec3(inv_world * glm::vec4(world_ray.origin, 1.0f));
        local_ray.dir = glm::normalize(glm::vec3(inv_world * glm::vec4(world_ray.dir, 0.0f)));

        if (intersect_aabb(current->collider_asset->aabb_min, current->collider_asset->aabb_max, local_ray)) {
            const auto& tris = current->collider_asset->triangles;
            for (size_t i = 0; i < tris.size(); i += 3) {
                hit_result hit = intersect_triangle(local_ray, tris[i], tris[i+1], tris[i+2]);
                if (hit.distance >= 0.0f) {
                    glm::vec3 world_point = glm::vec3(current->get_world_matrix() * glm::vec4(hit.point, 1.0f));
                    float world_dist = glm::length(world_point - world_ray.origin);

                    if (!closest_hit.has_hit() || world_dist < closest_hit.distance) {
                        closest_hit.hit_entity = current;
                        closest_hit.distance = world_dist;
                        closest_hit.point = world_point;
                        
                        glm::mat3 normal_mat = glm::transpose(glm::mat3(inv_world));
                        closest_hit.normal = glm::normalize(normal_mat * hit.normal);
                    }
                }
            }
        }
    }

    for (const auto& child : current->children) {
        cast_recursive(child, world_ray, closest_hit);
    }
}
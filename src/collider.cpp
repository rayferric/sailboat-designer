#include "collider.hpp"

void collider::load_from_glb(const std::filesystem::path& path) {
    tinygltf::Model gltf_scene;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    if (!loader.LoadBinaryFromFile(&gltf_scene, &err, &warn, path.string())) {
        std::cerr << "failed to load glTF: " << err << std::endl;
        return;
    }

    auto get_node_transform = [](const tinygltf::Node &node) -> glm::mat4 {
        glm::mat4 mat(1.0f);
        if (node.matrix.size() == 16) {
            mat = glm::make_mat4(node.matrix.data());
        } else {
            if (node.translation.size() == 3) {
                mat = glm::translate(mat, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            }
            if (node.rotation.size() == 4) {
                glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                mat *= glm::mat4_cast(q);
            }
            if (node.scale.size() == 3) {
                mat = glm::scale(mat, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
            }
        }
        return mat;
    };

    std::function<void(int, const glm::mat4 &)> process_node =
        [&](int node_idx, const glm::mat4 &parent_transform) {
            const auto &node = gltf_scene.nodes[node_idx];
            glm::mat4 local_transform = get_node_transform(node);
            glm::mat4 world_transform = parent_transform * local_transform;

            if (node.mesh >= 0) {
                const auto &mesh = gltf_scene.meshes[node.mesh];

                for (const auto &primitive : mesh.primitives) {
                    if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != 4) {
                        continue;
                    }

                    const auto &pos_accessor = gltf_scene.accessors[primitive.attributes.at("POSITION")];
                    const auto &pos_view = gltf_scene.bufferViews[pos_accessor.bufferView];
                    const auto &pos_buffer = gltf_scene.buffers[pos_view.buffer];
                    const float *positions = reinterpret_cast<const float *>(&pos_buffer.data[pos_view.byteOffset + pos_accessor.byteOffset]);

                    if (primitive.indices >= 0) {
                        const auto &idx_accessor = gltf_scene.accessors[primitive.indices];
                        const auto &idx_view = gltf_scene.bufferViews[idx_accessor.bufferView];
                        const auto &idx_buffer = gltf_scene.buffers[idx_view.buffer];
                        
                        const uint8_t* index_data = &idx_buffer.data[idx_view.byteOffset + idx_accessor.byteOffset];
                        
                        for (size_t i = 0; i < idx_accessor.count; i += 3) {
                            for(int j = 0; j < 3; ++j) {
                                uint32_t idx = 0;
                                if (idx_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                                    idx = reinterpret_cast<const uint16_t*>(index_data)[i + j];
                                } else if (idx_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                                    idx = reinterpret_cast<const uint32_t*>(index_data)[i + j];
                                } else if (idx_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                                    idx = index_data[i + j];
                                }
                                
                                glm::vec4 p(positions[idx * 3], positions[idx * 3 + 1], positions[idx * 3 + 2], 1.0f);
                                glm::vec3 p_world = glm::vec3(world_transform * p);
                                triangles.push_back(p_world);
                                aabb_min = glm::min(aabb_min, p_world);
                                aabb_max = glm::max(aabb_max, p_world);
                            }
                        }
                    } else {
                        for (size_t i = 0; i < pos_accessor.count; ++i) {
                            glm::vec4 p(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2], 1.0f);
                            glm::vec3 p_world = glm::vec3(world_transform * p);
                            triangles.push_back(p_world);
                            aabb_min = glm::min(aabb_min, p_world);
                            aabb_max = glm::max(aabb_max, p_world);
                        }
                    }
                }
            }

            for (int child_idx : node.children) {
                process_node(child_idx, world_transform);
            }
        };

    int default_scene = gltf_scene.defaultScene > -1 ? gltf_scene.defaultScene : 0;
    if (default_scene < gltf_scene.scenes.size()) {
        for (int node_idx : gltf_scene.scenes[default_scene].nodes) {
            process_node(node_idx, glm::mat4(1.0f));
        }
    }

    if (!triangles.empty()) {
        build_bvh();
    }
}

namespace {
    struct aabb {
        glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

        void grow(const glm::vec3 &p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        void grow(const aabb &other) {
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }

        float surface_area() const {
            glm::vec3 d = max - min;
            return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
        }
    };

    struct tri_info {
        aabb bounds;
        glm::vec3 centroid;
    };

    struct bvh_builder {
        std::vector<bvh_node> nodes;
        std::vector<uint32_t> tri_indices;
        const std::vector<glm::vec3>& triangles;
        uint32_t num_triangles;
        std::vector<tri_info> tri_infos;

        bvh_builder(const std::vector<glm::vec3>& tris) : triangles(tris), num_triangles(tris.size() / 3) {
            tri_indices.resize(num_triangles);
            tri_infos.resize(num_triangles);

            for (uint32_t i = 0; i < num_triangles; i++) {
                tri_indices[i] = i;

                const glm::vec3& v0 = tris[i * 3 + 0];
                const glm::vec3& v1 = tris[i * 3 + 1];
                const glm::vec3& v2 = tris[i * 3 + 2];

                tri_infos[i].bounds.grow(v0);
                tri_infos[i].bounds.grow(v1);
                tri_infos[i].bounds.grow(v2);
                tri_infos[i].centroid = (v0 + v1 + v2) / 3.0f;
            }

            nodes.reserve(num_triangles * 2);
        }

        uint32_t build(uint32_t start, uint32_t end) {
            uint32_t node_idx = nodes.size();
            nodes.emplace_back();

            aabb bounds;
            aabb centroid_bounds;
            for (uint32_t i = start; i < end; i++) {
                bounds.grow(tri_infos[tri_indices[i]].bounds);
                centroid_bounds.grow(tri_infos[tri_indices[i]].centroid);
            }

            uint32_t count = end - start;

            if (count <= 4) {
                nodes[node_idx].aabb_min = bounds.min;
                nodes[node_idx].aabb_max = bounds.max;
                nodes[node_idx].left_idx_or_tri_begin = start | 0x80000000;
                nodes[node_idx].right_idx_or_tri_count = count;
                return node_idx;
            }

            float best_cost = std::numeric_limits<float>::max();
            int best_axis = -1;
            float best_pos = 0.0f;

            const int num_bins = 16;
            struct bin {
                aabb bounds;
                uint32_t count = 0;
            };

            for (int axis = 0; axis < 3; axis++) {
                float axis_min = centroid_bounds.min[axis];
                float axis_max = centroid_bounds.max[axis];

                if (axis_min == axis_max) continue;

                bin bins[num_bins];
                float scale = num_bins / (axis_max - axis_min);
                
                for (uint32_t i = start; i < end; i++) {
                    uint32_t tri_idx = tri_indices[i];
                    float centroid = tri_infos[tri_idx].centroid[axis];
                    int bin_idx = std::min(num_bins - 1, (int)((centroid - axis_min) * scale));
                    bins[bin_idx].count++;
                    bins[bin_idx].bounds.grow(tri_infos[tri_idx].bounds);
                }

                float left_areas[num_bins - 1];
                float right_areas[num_bins - 1];
                uint32_t left_counts[num_bins - 1];
                uint32_t right_counts[num_bins - 1];

                aabb left_box, right_box;
                uint32_t left_sum = 0, right_sum = 0;

                for (int i = 0; i < num_bins - 1; i++) {
                    left_sum += bins[i].count;
                    left_box.grow(bins[i].bounds);
                    left_counts[i] = left_sum;
                    left_areas[i] = left_box.surface_area();
                }

                for (int i = num_bins - 1; i > 0; i--) {
                    right_sum += bins[i].count;
                    right_box.grow(bins[i].bounds);
                    right_counts[i - 1] = right_sum;
                    right_areas[i - 1] = right_box.surface_area();
                }

                for (int i = 0; i < num_bins - 1; i++) {
                    float cost = left_counts[i] * left_areas[i] + right_counts[i] * right_areas[i];
                    if (cost < best_cost) {
                        best_cost = cost;
                        best_axis = axis;
                        best_pos = axis_min + (axis_max - axis_min) * (i + 1) / num_bins;
                    }
                }
            }

            float leaf_cost = count * bounds.surface_area();
            if (best_cost >= leaf_cost || best_axis == -1) {
                nodes[node_idx].aabb_min = bounds.min;
                nodes[node_idx].aabb_max = bounds.max;
                nodes[node_idx].left_idx_or_tri_begin = start | 0x80000000;
                nodes[node_idx].right_idx_or_tri_count = count;
                return node_idx;
            }

            uint32_t mid = start;
            for (uint32_t i = start; i < end; i++) {
                if (tri_infos[tri_indices[i]].centroid[best_axis] < best_pos) {
                    std::swap(tri_indices[i], tri_indices[mid]);
                    mid++;
                }
            }

            if (mid == start || mid == end) {
                mid = (start + end) / 2;
            }

            uint32_t left_child = build(start, mid);
            uint32_t right_child = build(mid, end);

            nodes[node_idx].aabb_min = bounds.min;
            nodes[node_idx].aabb_max = bounds.max;
            nodes[node_idx].left_idx_or_tri_begin = left_child;
            nodes[node_idx].right_idx_or_tri_count = right_child;

            return node_idx;
        }
    };
} // namespace

void collider::build_bvh() {
    uint32_t num_triangles = triangles.size() / 3;
    if (num_triangles == 0) return;

    bvh_builder builder(triangles);
    builder.build(0, num_triangles);

    std::vector<glm::vec3> new_triangles;
    new_triangles.reserve(triangles.size());

    for (uint32_t i = 0; i < num_triangles; i++) {
        uint32_t old_idx = builder.tri_indices[i];
        new_triangles.push_back(triangles[old_idx * 3 + 0]);
        new_triangles.push_back(triangles[old_idx * 3 + 1]);
        new_triangles.push_back(triangles[old_idx * 3 + 2]);
    }

    triangles = std::move(new_triangles);
    bvh_nodes = std::move(builder.nodes);
}

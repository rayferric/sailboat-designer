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
}

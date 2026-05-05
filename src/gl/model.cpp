#include "model.hpp"

mesh::mesh() {
	// gen array
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// gen buf
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// setup attrib layout
	glVertexAttribPointer(
	    0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (GLvoid *)0
	);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(
	    1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (GLvoid *)(3 * sizeof(float))
	);
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(
	    2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (GLvoid *)(5 * sizeof(float))
	);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(
	    3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (GLvoid *)(8 * sizeof(float))
	);
	glEnableVertexAttribArray(3);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

mesh::~mesh() {
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

void mesh::load(const std::vector<vertex> &verts) {
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(
	    GL_ARRAY_BUFFER,
	    verts.size() * sizeof(vertex),
	    verts.data(),
	    GL_STATIC_DRAW
	);
	num_verts = verts.size();
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

//////////

void model::load_from_glb(const std::filesystem::path &path) {
	tinygltf::Model gltf_scene;
	tinygltf::TinyGLTF loader;
	std::string err, warn;

	if (!loader.LoadBinaryFromFile(&gltf_scene, &err, &warn, path.string())) {
		std::cerr << "failed to load glTF: " << err << std::endl;
		return;
	}

	// helper to get transform matrix from node
	auto get_node_transform = [](const tinygltf::Node &node) -> glm::mat4 {
		glm::mat4 mat(1.0f);
		if (node.matrix.size() == 16) {
			mat = glm::make_mat4(node.matrix.data());
		} else {
			if (node.translation.size() == 3) {
				mat = glm::translate(
				    mat,
				    glm::vec3(
				        node.translation[0],
				        node.translation[1],
				        node.translation[2]
				    )
				);
			}
			if (node.rotation.size() == 4) {
				glm::quat q(
				    node.rotation[3],
				    node.rotation[0],
				    node.rotation[1],
				    node.rotation[2]
				);
				mat *= glm::mat4_cast(q);
			}
			if (node.scale.size() == 3) {
				mat = glm::scale(
				    mat, glm::vec3(node.scale[0], node.scale[1], node.scale[2])
				);
			}
		}
		return mat;
	};

	// recursive node traversal
	std::function<void(int, const glm::mat4 &)> process_node =
	    [&](int node_idx, const glm::mat4 &parent_transform) {
		    const auto &node = gltf_scene.nodes[node_idx];
		    glm::mat4 local_transform = get_node_transform(node);
		    glm::mat4 world_transform = parent_transform * local_transform;

		    // process mesh if present
		    if (node.mesh >= 0) {
			    const auto &mesh = gltf_scene.meshes[node.mesh];

			    for (const auto &primitive : mesh.primitives) {
				    std::vector<vertex> vertices;

				    // extract positions
				    const auto &pos_accessor =
				        gltf_scene
				            .accessors[primitive.attributes.at("POSITION")];
				    const auto &pos_view =
				        gltf_scene.bufferViews[pos_accessor.bufferView];
				    const auto &pos_buffer =
				        gltf_scene.buffers[pos_view.buffer];
				    const float *positions = reinterpret_cast<const float *>(
				        &pos_buffer.data
				             [pos_view.byteOffset + pos_accessor.byteOffset]
				    );

				    // extract UVs
				    const float *uvs = nullptr;
				    if (primitive.attributes.count("TEXCOORD_0")) {
					    const auto &uv_accessor =
					        gltf_scene.accessors[primitive.attributes
					                                 .at("TEXCOORD_0")];
					    const auto &uv_view =
					        gltf_scene.bufferViews[uv_accessor.bufferView];
					    const auto &uv_buffer =
					        gltf_scene.buffers[uv_view.buffer];
					    uvs = reinterpret_cast<const float *>(
					        &uv_buffer.data
					             [uv_view.byteOffset + uv_accessor.byteOffset]
					    );
				    }

				    // extract normals
				    const float *normals = nullptr;
				    if (primitive.attributes.count("NORMAL")) {
					    const auto &norm_accessor =
					        gltf_scene
					            .accessors[primitive.attributes.at("NORMAL")];
					    const auto &norm_view =
					        gltf_scene.bufferViews[norm_accessor.bufferView];
					    const auto &norm_buffer =
					        gltf_scene.buffers[norm_view.buffer];
					    normals = reinterpret_cast<const float *>(
					        &norm_buffer.data
					             [norm_view.byteOffset +
					              norm_accessor.byteOffset]
					    );
				    }

				    // extract tangents
				    const float *tangents = nullptr;
				    if (primitive.attributes.count("TANGENT")) {
					    const auto &tan_accessor =
					        gltf_scene
					            .accessors[primitive.attributes.at("TANGENT")];
					    const auto &tan_view =
					        gltf_scene.bufferViews[tan_accessor.bufferView];
					    const auto &tan_buffer =
					        gltf_scene.buffers[tan_view.buffer];
					    tangents = reinterpret_cast<const float *>(
					        &tan_buffer.data
					             [tan_view.byteOffset + tan_accessor.byteOffset]
					    );
				    }

				    glm::mat3 normal_matrix = glm::transpose(
				        glm::inverse(glm::mat3(world_transform))
				    );

				    for (size_t i = 0; i < pos_accessor.count; ++i) {
					    vertex v = {};

					    // transform position
					    glm::vec4 pos =
					        world_transform * glm::vec4(
					                              positions[i * 3],
					                              positions[i * 3 + 1],
					                              positions[i * 3 + 2],
					                              1.0f
					                          );
					    v.pos[0] = pos.x;
					    v.pos[1] = pos.y;
					    v.pos[2] = pos.z;

					    // UVs
					    if (uvs) {
						    v.uv[0] = uvs[i * 2];
						    v.uv[1] = uvs[i * 2 + 1];
					    }

					    // transform normal
					    if (normals) {
						    glm::vec3 norm =
						        normal_matrix * glm::vec3(
						                            normals[i * 3],
						                            normals[i * 3 + 1],
						                            normals[i * 3 + 2]
						                        );
						    norm = glm::normalize(norm);
						    v.norm[0] = norm.x;
						    v.norm[1] = norm.y;
						    v.norm[2] = norm.z;
					    }

					    // transform tangent
					    if (tangents) {
						    glm::vec3 tan =
						        normal_matrix * glm::vec3(
						                            tangents[i * 4],
						                            tangents[i * 4 + 1],
						                            tangents[i * 4 + 2]
						                        );
						    tan = glm::normalize(tan);
						    v.tangent[0] = tan.x;
						    v.tangent[1] = tan.y;
						    v.tangent[2] = tan.z;
					    }

					    vertices.push_back(v);
				    }

				    // after building vertices vector
				    if (primitive.indices >= 0) {
					    const auto &idx_accessor =
					        gltf_scene.accessors[primitive.indices];
					    const auto &idx_view =
					        gltf_scene.bufferViews[idx_accessor.bufferView];
					    const auto &idx_buffer =
					        gltf_scene.buffers[idx_view.buffer];

					    std::vector<vertex> indexed_verts;

					    // handle different index types
					    if (idx_accessor.componentType ==
					        TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
						    const uint16_t *indices =
						        reinterpret_cast<const uint16_t *>(
						            &idx_buffer.data
						                 [idx_view.byteOffset +
						                  idx_accessor.byteOffset]
						        );
						    for (size_t i = 0; i < idx_accessor.count; ++i) {
							    indexed_verts.push_back(vertices[indices[i]]);
						    }
					    } else if (idx_accessor.componentType ==
					               TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
						    const uint32_t *indices =
						        reinterpret_cast<const uint32_t *>(
						            &idx_buffer.data
						                 [idx_view.byteOffset +
						                  idx_accessor.byteOffset]
						        );
						    for (size_t i = 0; i < idx_accessor.count; ++i) {
							    indexed_verts.push_back(vertices[indices[i]]);
						    }
					    }

					    vertices = std::move(indexed_verts);
				    }

				    part &p = parts.emplace_back();
				    p.mesh.load(vertices);

				    // load material if available
				    if (primitive.material >= 0) {
					    const auto &mat =
					        gltf_scene.materials[primitive.material];

					    // color factor
					    auto &bcf = mat.pbrMetallicRoughness.baseColorFactor;
					    p.mat.color =
					        (bcf.size() == 4)
					            ? glm::vec4(bcf[0], bcf[1], bcf[2], bcf[3])
					            : glm::vec4(1.0f);

					    // metallic / roughness factors
					    p.mat.metallic      = (float)mat.pbrMetallicRoughness.metallicFactor;
					    p.mat.roughness     = (float)mat.pbrMetallicRoughness.roughnessFactor;
					    p.mat.double_sided  = mat.doubleSided;

					    // color texture
					    if (mat.pbrMetallicRoughness.baseColorTexture.index >=
					        0) {
						    const auto &tex =
						        gltf_scene
						            .textures[mat.pbrMetallicRoughness
						                          .baseColorTexture.index];
						    const auto &img = gltf_scene.images[tex.source];
						    GLenum format =
						        (img.component == 4) ? GL_RGBA : GL_RGB;
						    p.mat.color_tex.emplace();
						    p.mat.color_tex.value().load(
						        img.image.data(), img.width, img.height, format
						    );
					    } else {
						    // default white texture
						    p.mat.color_tex.emplace();
							uint8_t white_pixel[] = {255, 255, 255};
							p.mat.color_tex.value().load(white_pixel, 1, 1, GL_RGB);
					    }
				    }
			    }
		    }

		    // recurse to children
		    for (int child_idx : node.children) {
			    process_node(child_idx, world_transform);
		    }
	    };

	// process all scenes (typically just one)
	for (const auto &scene : gltf_scene.scenes) {
		for (int node_idx : scene.nodes) {
			process_node(node_idx, glm::mat4(1.0f));
		}
	}
}

void model::draw_parts(uniform_buffer &ubo) {
	for (auto &part : parts) {
		if (part.mat.double_sided) {
			glDisable(GL_CULL_FACE);
		} else {
			glEnable(GL_CULL_FACE);
		}

		if (part.mat.color_tex.has_value()) {
			part.mat.color_tex.value().bind(0);
		}

		ubo.update(part.mat.color, glm::vec4(part.mat.metallic, part.mat.roughness, 0.0f, 0.0f));

		auto &m = part.mesh;
		glBindVertexArray(m.vao);
		glDrawArrays(GL_TRIANGLES, 0, m.num_verts);
	}
	glEnable(GL_CULL_FACE);
}

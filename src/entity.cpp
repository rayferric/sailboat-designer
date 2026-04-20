#include "entity.hpp"

void entity::add_child(std::shared_ptr<entity> child) {
    child->parent = shared_from_this();
    children.push_back(child);
}

glm::mat4 entity::get_local_matrix() const {
    return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
}

glm::mat4 entity::get_world_matrix() const {
    glm::mat4 local = get_local_matrix();
    if (auto p = parent.lock()) {
        return p->get_world_matrix() * local;
    }
    return local;
}

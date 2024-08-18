#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

struct Transform {

    glm::vec3 position;
    glm::vec3 scale;
    glm::quat rotation;
};
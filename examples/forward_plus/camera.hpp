#pragma once
#include "core/vk_include.hpp"


namespace ForwardPlus
{
class Camera
{
public:
    static glm::mat4 view(const glm::vec3& eye) { return glm::lookAtRH(eye, glm::vec3(0), glm::vec3(0, 0, 1)); }
};
}    // namespace ForwardPlus

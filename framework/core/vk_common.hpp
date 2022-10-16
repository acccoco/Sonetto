#pragma once
#include "core/vk_include.hpp"


namespace Hiss
{


// pipeline stage 以及 access flag 的简单组合
struct StageAccess
{
    vk::PipelineStageFlags stage  = {};
    vk::AccessFlags        access = {};
};


// pipeline stage 和 semaphore 的简单组合
struct StageSemaphore
{
    vk::PipelineStageFlags stage     = {};
    vk::Semaphore          semaphore = {};
};


/**
 *
 * @param fov_deg
 * @param aspect width / height
 * @param near
 * @param far
 * @return
 */
inline glm::mat4 perspective(float fov_deg, float aspect, float near, float far)
{
    auto mat = glm::perspectiveRH_ZO(glm::radians(fov_deg), aspect, near, far);
    mat[1][1] *= -1;
    return mat;
}

}    // namespace Hiss
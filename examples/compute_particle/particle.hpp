#pragma once
#include "core/vk_common.hpp"


namespace ParticleCompute
{


// 粒子的定义
struct Particle    // std430
{
    alignas(16) glm::vec4 pos;    // xyz: position, w: mass
    alignas(16) glm::vec4 vel;    // xyz: velocity, w: uv coord
};
}    // namespace ParticleCompute
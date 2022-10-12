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


}    // namespace Hiss
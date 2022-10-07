#pragma once


#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>


#define VK_ENABLE_BETA_EXTENSIONS    // vulkan_beta.h，在 metal 上运行 vulkan，需要这个
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_UNION_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_SETTERS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

/* vma 有 include vulkan.h，为了确保宏定义开关，需要在 vulkan.h 之后 include  vma */
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"


/* 需要在 vulkan 之后被 include，内部的一些声明需要来自 vulkan 的 macro */
#include <GLFW/glfw3.h>


#define GLM_FORCE_RADIANS
/* 透视投影矩阵生成的深度范围是 [-1.0, 1.0]，这里可以将其改成 [0.0, 1.0] */
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
/* 让 glm 的类型支持 std::hash<...>() */
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>


#include "stb_image.h"


#define BITS_CONTAIN(bits_a, bits_b) ((bits_a & bits_b) == bits_b)


namespace Hiss
{

/* depth format 中是否包含 stencil */
inline bool has_stencil_compnent(const vk::Format& format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}


inline bool is_depth_only_format(vk::Format format)
{
    return format == vk::Format::eD16Unorm || format == vk::Format::eD32Sfloat;
}


inline bool is_depth_stencil_format(vk::Format format)
{
    return format == vk::Format::eD16UnormS8Uint || format == vk::Format::eD24UnormS8Uint
        || format == vk::Format::eD32SfloatS8Uint || is_depth_only_format(format);
}


bool check_layers(const std::vector<const char*>& layers);


/* 仅用于 DEBUG，包含所有的访问 flag */
const vk::AccessFlags ALL_ACCESS =
        vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eIndexRead
        | vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eUniformRead
        | vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite
        | vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite
        | vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite
        | vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eHostRead
        | vk::AccessFlagBits::eHostWrite;


/* 无 mipmap 的 image 的 subresource range，swapchain 的 image 就符合这个 */
const vk::ImageSubresourceRange COLOR_SUBRESOURCE_RANGE = {
        .aspectMask     = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
};


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
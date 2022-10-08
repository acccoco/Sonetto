#pragma once
#include "vk/vk_include.hpp"


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
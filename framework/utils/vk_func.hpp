#pragma once
#include "core/vk_include.hpp"
#include "engine/texture.hpp"


namespace Hiss::Initial
{
inline vk::Viewport viewport(const vk::Extent2D& extent, float min_depth = 0.f, float max_depth = 1.f)
{
    return vk::Viewport{
            .x        = 0.f,
            .y        = 0.f,
            .width    = (float) extent.width,
            .height   = (float) extent.height,
            .minDepth = min_depth,
            .maxDepth = max_depth,
    };
}


struct BindingInfo
{
    vk::DescriptorType   type;
    vk::ShaderStageFlags stage;
    uint32_t             count = 1;
};

inline std::vector<vk::DescriptorSetLayoutBinding> descriptor_bindings(const std::vector<BindingInfo>& info)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.resize(info.size());

    for (uint32_t i = 0; i < info.size(); ++i)
    {
        bindings[i] = vk::DescriptorSetLayoutBinding{
                .binding         = i,
                .descriptorType  = info[i].type,
                .descriptorCount = info[i].count,
                .stageFlags      = info[i].stage,
        };
    }

    return bindings;
}


inline vk::DescriptorSetLayout descriptor_set_layout(vk::Device device, const std::vector<BindingInfo>& info)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.resize(info.size());

    for (uint32_t i = 0; i < info.size(); ++i)
    {
        bindings[i] = vk::DescriptorSetLayoutBinding{
                .binding         = i,
                .descriptorType  = info[i].type,
                .descriptorCount = info[i].count,
                .stageFlags      = info[i].stage,
        };
    }

    return device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = (uint32_t) bindings.size(),
            .pBindings    = bindings.data(),
    });
}


inline vk::RenderingAttachmentInfo depth_attach_info(vk::ImageView view = VK_NULL_HANDLE)
{
    return vk::RenderingAttachmentInfo{
            .imageView   = view,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eDontCare,
            .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
    };
}


inline vk::RenderingAttachmentInfo color_resolve_attach(vk::ImageView view)
{
    return vk::RenderingAttachmentInfo{
            .imageView          = view,
            .imageLayout        = vk::ImageLayout::eColorAttachmentOptimal,
            .resolveMode        = vk::ResolveModeFlagBits::eAverage,
            .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp             = vk::AttachmentLoadOp::eClear,
            .storeOp            = vk::AttachmentStoreOp::eDontCare,
            .clearValue         = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.f}}},
    };
}


inline vk::RenderingAttachmentInfo color_attach_info()
{
    return vk::RenderingAttachmentInfo{
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.f}}},
    };
}


inline vk::RenderingInfo render_info(vk::RenderingAttachmentInfo& color_attach,
                                     vk::RenderingAttachmentInfo& depth_attach, const vk::Extent2D& extent,
                                     const vk::Offset2D& offset = {0, 0})
{
    return vk::RenderingInfo{
            .renderArea           = {.offset = offset, .extent = extent},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &color_attach,
            .pDepthAttachment     = &depth_attach,
    };
}


inline vk::RenderingInfo render_info(vk::RenderingAttachmentInfo& depth_attach, const vk::Extent2D& extent,
                                     const vk::Offset2D& offset = {0, 0})
{
    return vk::RenderingInfo{
            .renderArea       = {.offset = offset, .extent = extent},
            .layerCount       = 1,
            .pDepthAttachment = &depth_attach,
    };
}


inline vk::DescriptorSetLayout descriptor_set_layout(vk::Device                                   device,
                                                     std::vector<vk::DescriptorSetLayoutBinding>& bindings)
{
    return device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = (uint32_t) bindings.size(),
            .pBindings    = bindings.data(),
    });
}


struct DescriptorWrite
{
    vk::DescriptorType type{};
    Hiss::Buffer*      buffer{};
    Hiss::Image2D*     image{};
    vk::Sampler        sampler;
    int                binding = -1;
};


/**
 * 绑定 descriptor set 与 buffer
 * @details 支持的类型有：storage buffer，uniform buffer，combined sampler image，storage image
 */
inline void descriptor_set_write(vk::Device device, vk::DescriptorSet descriptor_set,
                                 const std::vector<DescriptorWrite>& writes)
{
    std::vector<vk::WriteDescriptorSet> vk_writes;
    vk_writes.resize(writes.size());

    std::vector<vk::DescriptorBufferInfo*> buffer_infos;
    std::vector<vk::DescriptorImageInfo*>  image_infos;
    for (uint32_t i = 0; i < writes.size(); ++i)
    {
        auto type = writes[i].type;

        vk::WriteDescriptorSet write_set = {
                .dstSet          = descriptor_set,
                .dstBinding      = writes[i].binding == -1 ? i : writes[i].binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = type,
        };


        // descriptor 是 uniform 或者 storage buffer
        if (type == vk::DescriptorType::eUniformBuffer || type == vk::DescriptorType::eStorageBuffer)
        {
            auto p_buffer_info = new vk::DescriptorBufferInfo{
                    .buffer = writes[i].buffer->vkbuffer(),
                    .offset = 0,
                    .range  = writes[i].buffer->size(),
            };
            buffer_infos.push_back(p_buffer_info);
            write_set.pBufferInfo = p_buffer_info;
        }

        // descriptor 是 texture：有 image 和 sampler
        else if (type == vk::DescriptorType::eCombinedImageSampler)
        {
            auto p_image_info = new vk::DescriptorImageInfo{
                    .sampler     = writes[i].sampler,
                    .imageView   = writes[i].image->vkview(),
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            };
            image_infos.push_back(p_image_info);
            write_set.pImageInfo = p_image_info;
        }

        // 是 storage image
        else if (type == vk::DescriptorType::eStorageImage)
        {
            auto p_image_info = new vk::DescriptorImageInfo{
                    .imageView   = writes[i].image->vkview(),
                    .imageLayout = vk::ImageLayout::eGeneral,
            };
            image_infos.push_back(p_image_info);
            write_set.pImageInfo = p_image_info;
        }

        // 没有匹配上
        else
            throw std::runtime_error("unsupported descriptor type: " + to_string(type));

        vk_writes[i] = write_set;
    }

    // 进行绑定
    device.updateDescriptorSets(vk_writes, {});


    // 销毁资源
    for (auto image_info: image_infos)
        delete image_info;
    for (auto buffer_info: buffer_infos)
        delete buffer_info;
}


inline vk::PipelineLayout pipeline_layout(vk::Device                                  device,
                                          const std::vector<vk::DescriptorSetLayout>& descriptor_set_layouts,
                                          const std::vector<vk::PushConstantRange>&   push_constants = {})
{
    return device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount         = (uint32_t) descriptor_set_layouts.size(),
            .pSetLayouts            = descriptor_set_layouts.data(),
            .pushConstantRangeCount = (uint32_t) push_constants.size(),
            .pPushConstantRanges    = push_constants.data(),
    });
}


inline vk::Pipeline compute_pipeline(vk::Device device, vk::PipelineLayout layout,
                                     const vk::PipelineShaderStageCreateInfo& shader_stage)
{
    vk::Result   result;
    vk::Pipeline pipeline;
    std::tie(result, pipeline) = device.createComputePipeline(nullptr, vk::ComputePipelineCreateInfo{
                                                                               .stage  = shader_stage,
                                                                               .layout = layout,
                                                                       });
    vk::resultCheck(result, "compute pipeline create.");
    return pipeline;
}


/**
 * 创建一个比较通用的：linear，mirrored repeat，无 mipmap
 */
inline vk::Sampler sampler(const Hiss::Device& device)
{
    return device.vkdevice().createSampler(vk::SamplerCreateInfo{
            .magFilter  = vk::Filter::eLinear,
            .minFilter  = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,

            // 纹理坐标超出 [0, 1) 后，如何处理
            .addressModeU = vk::SamplerAddressMode::eClampToEdge,
            .addressModeV = vk::SamplerAddressMode::eClampToEdge,
            .addressModeW = vk::SamplerAddressMode::eClampToEdge,

            .mipLodBias = 0.f,

            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy    = device.gpu().properties().limits.maxSamplerAnisotropy,

            // 在 PCF shadow map 中会用到
            .compareEnable = VK_FALSE,
            .compareOp     = vk::CompareOp::eAlways,

            // 用于 clamp LOD 级别的
            .minLod = 0.f,
            .maxLod = static_cast<float>(1),

            .borderColor             = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = VK_FALSE,
    });
}


}    // namespace Hiss::Initial
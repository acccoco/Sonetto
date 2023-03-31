#pragma once
#include "engine/engine.hpp"
#include "vk_func.hpp"


namespace Hiss
{

/**
 * 对 descriptor set layout 的简单包装
 */
struct DescriptorLayout
{
    DescriptorLayout(Hiss::Device& device, const std::vector<Hiss::Initial::BindingInfo>& bindings)
        : device(device)
    {
        this->bindings = bindings;

        layout = Hiss::Initial::descriptor_set_layout(device.vkdevice(), bindings);
    }

    ~DescriptorLayout() { device.vkdevice().destroy(layout); }


    static std::shared_ptr<DescriptorLayout> create_material_layout(Hiss::Device& device)
    {
        if (!material_layout_singleton)
        {
            material_layout_singleton = std::make_shared<DescriptorLayout>(
                    device, std::vector<Hiss::Initial::BindingInfo>{
                                    {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                            });
        }

        return material_layout_singleton;
    }

    inline static std::shared_ptr<DescriptorLayout> material_layout_singleton;


    Hiss::Device&                           device;
    vk::DescriptorSetLayout                 layout;
    std::vector<Hiss::Initial::BindingInfo> bindings;
};


/**
 * 对 descriptor set 的简单包装
 */
struct DescriptorSet
{
    struct WriteContent
    {
        Hiss::Buffer*  buffer{};
        Hiss::Image2D* image{};
        vk::Sampler    sampler;
        int            binding = -1;
    };


    explicit DescriptorSet(Hiss::Engine& engine, const std::shared_ptr<DescriptorLayout>& layout,
                           const std::string& name)
        : device(layout->device),
          layout(layout)
    {
        vk_descriptor_set = engine.create_descriptor_set(layout->layout, name);
    }


    void write(const std::vector<WriteContent>& contents)
    {
        for (int i = 0; i < contents.size(); ++i)
        {
            uint32_t idx = contents[i].binding == -1 ? i : contents[i].binding;

            write(contents[i], idx);
        }
    }


    void write(const WriteContent& content, uint32_t idx)
    {
        assert(layout->bindings.size() > idx);

        auto type = layout->bindings[idx].type;

        vk::WriteDescriptorSet write_info = {
                .dstSet          = vk_descriptor_set,
                .dstBinding      = idx,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = type,
        };

        vk::DescriptorImageInfo  image_info;
        vk::DescriptorBufferInfo buffer_info;


        // 下面是 descriptor 的不同类型
        if (type == vk::DescriptorType::eUniformBuffer || type == vk::DescriptorType::eStorageBuffer)
        {
            buffer_info.buffer = content.buffer->vkbuffer();
            buffer_info.offset = 0;
            buffer_info.range  = content.buffer->size();

            write_info.pBufferInfo = &buffer_info;
        }
        else if (type == vk::DescriptorType::eCombinedImageSampler)
        {
            image_info.sampler     = content.sampler;
            image_info.imageView   = content.image->vkview();
            image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            write_info.pImageInfo = &image_info;
        }
        else if (type == vk::DescriptorType::eStorageImage)
        {
            image_info.imageView   = content.image->vkview();
            image_info.imageLayout = vk::ImageLayout::eGeneral;

            write_info.pImageInfo = &image_info;
        }
        else
            throw std::runtime_error(fmt::format("unsuported descriptor type: {}", to_string(type)));

        device.vkdevice().updateDescriptorSets(write_info, {});
    }


    Hiss::Device&                     device;
    std::shared_ptr<DescriptorLayout> layout;
    vk::DescriptorSet                 vk_descriptor_set;
};

}    // namespace Hiss
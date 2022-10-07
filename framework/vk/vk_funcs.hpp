#pragma once
#include "vk/vk_common.hpp"


namespace Hiss
{
inline vk::DescriptorSetLayout create_descriptor_set_layout(const vk::Device&                                  device,
                                                            const std::vector<vk::DescriptorSetLayoutBinding>& bindings)
{
    return device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings    = bindings.data(),
    });
}
}    // namespace Hiss
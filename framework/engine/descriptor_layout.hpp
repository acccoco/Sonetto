#pragma once
#include "core/vk_include.hpp"
#include "utils/vk_func.hpp"


namespace Hiss
{
struct DescriptorLayout
{
    DescriptorLayout(Device& device, const std::vector<Hiss::Initial::BindingInfo>& info)
        : device(device)
    {
        vk_descriptor_layout = Hiss::Initial::descriptor_set_layout(device.vkdevice(), info);
    }
    ~DescriptorLayout() { device.vkdevice().destroy(vk_descriptor_layout); }

    Device&                 device;
    vk::DescriptorSetLayout vk_descriptor_layout;
};

}    // namespace Hiss
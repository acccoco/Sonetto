#pragma once
#include <memory>

#include "./window.hpp"
#include "./instance.hpp"
#include "./vk_config.hpp"
#include "./gpu.hpp"
#include "./device.hpp"


namespace Hiss
{
class VulkanCore
{
public:
    VulkanCore(const std::string& name, Hiss::Window& window)
        : _window(window)
    {
        /**
         * instance
         */
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        _instance = std::make_unique<Instance>(name, &_debug_utils_messenger_info);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance->vkinstance());


        /**
         * vulkan validation 的 debug messenger
         */
        _debug_messenger = _instance->vkinstance().createDebugUtilsMessengerEXT(_debug_utils_messenger_info);


        _surface = window.create_surface(_instance->vkinstance());


        /**
         * physical device
         */
        auto vk_physical_device = _instance->gpu_pick(_surface);
        if (!vk_physical_device.has_value())
            throw std::runtime_error("no suitable gpu found.");
        _gpu = std::make_unique<GPU>(vk_physical_device.value(), _surface);


        /**
         * 创建 logical device
         */
        _device = std::make_unique<Device>(*_gpu);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_device->vkdevice());


        /**
         * descriptor pool
         */
        create_descriptor_pool();
    }


    ~VulkanCore()
    {
        _device->vkdevice().destroy(_descriptor_pool);
        _device.reset();
        _gpu.reset();
        _instance->vkinstance().destroy(_surface);
        _instance->vkinstance().destroy(_debug_messenger);
        _instance.reset();
    }


private:
    Window& _window;

    std::unique_ptr<Instance> _instance;
    std::unique_ptr<GPU>      _gpu;
    std::unique_ptr<Device>   _device;

    vk::DebugUtilsMessengerEXT _debug_messenger;
    vk::SurfaceKHR             _surface;
    vk::DescriptorPool         _descriptor_pool;


    void create_descriptor_pool()
    {
        _descriptor_pool = _device->vkdevice().createDescriptorPool(vk::DescriptorPoolCreateInfo{
                .maxSets       = descriptor_set_max_number,
                .poolSizeCount = static_cast<uint32_t>(pool_size.size()),
                .pPoolSizes    = pool_size.data(),
        });

        spdlog::info("[descriptor pool] descriptor set max number: {}", descriptor_set_max_number);
        for (auto& item: pool_size)
            spdlog::info("[descriptor pool] max number: ({}, {})", to_string(item.type), item.descriptorCount);
    }
};

}    // namespace Hiss
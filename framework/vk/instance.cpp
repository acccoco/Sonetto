#include <utility>

#include "vk/instance.hpp"
#include "vk/vk_config.hpp"


Hiss::Instance::Instance(const std::string&                          app_name,
                         const vk::DebugUtilsMessengerCreateInfoEXT* debug_utils_messenger_info)
{
    vk::ApplicationInfo app_info = {
            .pApplicationName   = app_name.c_str(),
            .applicationVersion = VK_MAKE_VERSION(1, 1, 4),
            .pEngineName        = "Hiss 🥵 Engine",
            .engineVersion      = VK_MAKE_VERSION(5, 1, 4),

            .apiVersion = APP_VK_VERSION,
    };


    auto extensions = get_instance_extensions();
    auto layers     = get_layers();
    if (!Hiss::check_layers(layers))
        throw std::runtime_error("layers unsupported.");


    vk::InstanceCreateInfo instance_info = {
            /**
             * generate 和 destroy 期间 validation layer 没有启用，
             * 可以通过这个方法来捕获这两个阶段的 event
             */
            .pNext = reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(debug_utils_messenger_info),

            .flags                   = INSTANCE_FLAGS,
            .pApplicationInfo        = &app_info,
            .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
            .ppEnabledLayerNames     = layers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
    };

    _instance = vk::createInstance(instance_info);
}


Hiss::Instance::~Instance()
{
    _instance.destroy();
}


std::optional<vk::PhysicalDevice> Hiss::Instance::gpu_pick(vk::SurfaceKHR surface)
{
    for (auto physical_device: _instance.enumeratePhysicalDevices())
    {
        auto queue_properties = physical_device.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queue_properties.size(); ++i)
        {
            if (physical_device.getSurfaceSupportKHR(i, surface))
                return physical_device;
        }
    }
    return std::nullopt;
}

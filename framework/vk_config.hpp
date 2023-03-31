#pragma once
#include <vector>
#include "core/vk_common.hpp"


namespace Hiss
{

/* MoltenVk 是 Vulkan 1.1 的实现 */
const uint32_t APP_VK_VERSION = VK_API_VERSION_1_1;


// instance 需要的 layers
inline std::vector<const char*> get_layers()
{
    return {
            "VK_LAYER_KHRONOS_validation",

            /**
             * 已知的问题：
             * 使用 portability device 有两种方法：
             *  1. 开启 render doc layer
             *  2. 使用 portablity extension
             * 注1：render doc layer 不支持 portability extension
             * 注2：可以在 vulkan config 应用中覆盖全局的 layer 配置
             */
            // "VK_LAYER_RENDERDOC_Capture",

            /**
             * synchronization2 需要硬件支持，还需要 vulkan api version 在 1.3 以上
             */
            // "VK_LAYER_KHRONOS_synchronization2",
    };
}


// glfw 需要的 instance extension
inline std::vector<const char*> get_instance_extensions_glfw()
{
    uint32_t     extension_cnt = 0;
    const char** extensions    = glfwGetRequiredInstanceExtensions(&extension_cnt);

    std::vector<const char*> extension_list;
    for (int i = 0; i < extension_cnt; ++i)
        extension_list.push_back(extensions[i]);
    return extension_list;
}


// 项目需要的 instance extension
inline std::vector<const char*> get_instance_extensions()
{

    std::vector<const char*> extensions = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,                // 可以将 validation 信息打印出来
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,    // 基于 metal API 的 vulkan 实现需要这些扩展
    };

    auto glfw_extensions = get_instance_extensions_glfw();
    extensions.insert(extensions.end(), glfw_extensions.begin(), glfw_extensions.end());


    return extensions;
}


/**
 * 表示 vulkan 除了枚举出默认的 physical device 外，
 * 还会枚举出符合 vulkan 可移植性的 physical device
 * 基于 metal 的 vulkan 需要这个
 */
const vk::InstanceCreateFlags INSTANCE_FLAGS = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;


inline std::vector<const char*> get_device_extensions()
{
    return {
            /* 这是一个临时的扩展（vulkan_beta.h)，在 metal API 上模拟 vulkan 需要这个扩展 */
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,

            /* 可以将渲染结果呈现到 window surface 上 */
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,

            // dynamic render 需要的扩展
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
            VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };
}


// 应用所需的 device features
inline vk::PhysicalDeviceFeatures get_device_features()
{
    return vk::PhysicalDeviceFeatures{
            .tessellationShader = VK_TRUE,
            .sampleRateShading  = VK_TRUE,
            .fillModeNonSolid   = VK_TRUE,
            .samplerAnisotropy  = VK_TRUE,
    };
}


// validation 的 debug 回调函数
static vk::Bool32 validation_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                            VkDebugUtilsMessageTypeFlagsEXT,
                                            const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void*)
{
    switch (message_severity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn("[validation] {}\n", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error("[validation] {}\n", callback_data->pMessage);
            break;
        default: spdlog::info("[validation] {}", callback_data->pMessage);
    }

    return VK_FALSE;
}


// validation debug log 的级别
const vk::DebugUtilsMessengerCreateInfoEXT _debug_utils_messenger_info = {
        .messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                     | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                     | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
        .pfnUserCallback = validation_debug_callback,
};


// ==============================================================
// descriptor pool 相关的配置
// ==============================================================
const uint32_t                            descriptor_set_max_number = 1024;
const std::vector<vk::DescriptorPoolSize> pool_size                 = {
        {vk::DescriptorType::eUniformBuffer, 1024},
        {vk::DescriptorType::eStorageBuffer, 1024},
        {vk::DescriptorType::eCombinedImageSampler, 1024},
        {vk::DescriptorType::eStorageImage, 1024},
};

}    // namespace Hiss
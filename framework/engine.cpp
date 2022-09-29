#include "engine.hpp"
#include "utils/tools.hpp"
#include "vk/vk_config.hpp"
#include "proj_profile.hpp"


/**
 * vulkan 相关函数的 dynamic loader
 * 这是变量定义的位置，vulkan.hpp 会通过 extern 引用这个全局变量
 * 需要初始化三次：
 *   第一次传入 vkGetInstanceProcAddr
 *   第二次传入 instance
 *   第三次传入 device
 */
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE


void Hiss::Engine::resize()
{
    _window->on_resize();
    _swapchain    = Swapchain::resize(_swapchain, *device._ptr, *_window, _surface);
    frame_manager = Hiss::FrameManager::resize(frame_manager._ptr, *device._ptr, *_swapchain);
}


void Hiss::Engine::prepare()
{
    _window = new Window(name(), WINDOW_WIDTH, WINDOW_HEIGHT);

    timer._value.start();

    // 创建 vulkan 应用的 instance
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    _instance = new Instance(name(), &_debug_utils_messenger_info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance->vkinstance());
    spdlog::info("[engine] instance created.");


    // 创建 validation 的 debug messenger
    _debug_messenger = _instance->vkinstance().createDebugUtilsMessengerEXT(_debug_utils_messenger_info);


    // 创建 surface
    _surface = _window->create_surface(_instance->vkinstance());


    // 创建 physical device
    auto physical_device = _instance->gpu_pick(_surface);
    if (!physical_device.has_value())
        throw std::runtime_error("no suitable gpu found.");
    _physical_device = new GPU(physical_device.value(), _surface);
    spdlog::info("[engine] physical device created.");


    // 创建 logical device
    device = new Device(*_physical_device);
    spdlog::info("[engine] device created.");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device().vkdevice());

    init_vma();

    // 创建 swapchain
    _swapchain = new Swapchain(*device._ptr, *_window, _surface);


    frame_manager = new Hiss::FrameManager(*device._ptr, *_swapchain);

    shader_loader = new ShaderLoader(*device._ptr);
}


void Hiss::Engine::init_vma()
{
    VmaVulkanFunctions vulkan_funcs = {
            .vkGetInstanceProcAddr = &vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr   = &vkGetDeviceProcAddr,
    };

    VmaAllocatorCreateInfo vma_allocator_info = {
            .physicalDevice   = _physical_device->vkgpu(),
            .device           = device().vkdevice(),
            .pVulkanFunctions = &vulkan_funcs,
            .instance         = _instance->vkinstance(),
            .vulkanApiVersion = APP_VK_VERSION,
    };

    vmaCreateAllocator(&vma_allocator_info, &allocator);
}


void Hiss::Engine::clean()
{
    DELETE(shader_loader._value);
    DELETE(frame_manager._ptr);
    DELETE(_swapchain);

    // 销毁 vma 的分配器
    vmaDestroyAllocator(allocator);

    DELETE(device._ptr);
    DELETE(_physical_device);
    _instance->vkinstance().destroy(_surface);
    _instance->vkinstance().destroy(_debug_messenger);
    DELETE(_instance);
    DELETE(_window);
}


void Hiss::Engine::preupdate() noexcept
{
    frame_manager._ptr->acquire_frame();
}


void Hiss::Engine::postupdate() noexcept
{
    frame_manager._ptr->submit_frame();
}


Hiss::Image2D* Hiss::Engine::create_depth_image() const
{
    return new Hiss::Image2D(allocator, *device._ptr,
                             Hiss::Image2D::Info{
                                     .format       = depth_format(),
                                     .extent       = extent(),
                                     .usage        = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                     .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                     .aspect       = vk::ImageAspectFlagBits::eDepth,
                                     .init_layout  = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                             });
}

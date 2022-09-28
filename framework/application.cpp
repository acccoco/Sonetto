#include "application.hpp"
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


void Hiss::Application::resize()
{
    _window->on_resize();
    _swapchain    = Swapchain::resize(_swapchain, device(), *_window, _surface);
    frame_manager = Hiss::FrameManager::resize(frame_manager._ptr, device(), *_swapchain);
}


void Hiss::Application::prepare()
{
    _window = new Window(name.get(), WINDOW_WIDTH, WINDOW_HEIGHT);

    timer._value.start();

    // 创建 vulkan 应用的 instance
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    _instance = new Instance(name.get(), &_debug_utils_messenger_info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance->vkinstance());
    spdlog::info("instance created.");


    // 创建 validation 的 debug messenger
    _debug_messenger = _instance->vkinstance().createDebugUtilsMessengerEXT(_debug_utils_messenger_info);


    // 创建 surface
    _surface = _window->create_surface(_instance->vkinstance());


    // 创建 physical device
    auto physical_device = _instance->gpu_pick(_surface);
    if (!physical_device.has_value())
        throw std::runtime_error("no suitable gpu found.");
    _physical_device = new GPU(physical_device.value(), _surface);
    spdlog::info("physical device created.");


    // 创建 logical device
    device = new Device(*_physical_device);
    spdlog::info("device created.");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device().vkdevice());

    init_vma();

    // 创建 swapchain
    _swapchain = new Swapchain(device(), *_window, _surface);
    spdlog::info("image count: {}", _swapchain->get_image_number());


    frame_manager = new Hiss::FrameManager(device(), *_swapchain);

    shader_loader = new ShaderLoader(device());
}


void Hiss::Application::init_vma()
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


void Hiss::Application::clean()
{
    DELETE(shader_loader._ptr);
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


void Hiss::Application::perupdate() noexcept
{
    _window->poll_event();
    current_frame = frame_manager._ptr->acquire_frame();
}


void Hiss::Application::postupdate() noexcept
{
    frame_manager._ptr->submit_frame(current_frame._ptr);
}

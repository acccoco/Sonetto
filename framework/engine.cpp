#include "engine.hpp"
#include "utils/tools.hpp"
#include "vk/vk_config.hpp"
#include "proj_config.hpp"


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
    _swapchain     = Swapchain::resize(_swapchain, *_device, *_window, _surface);
    _frame_manager = Hiss::FrameManager::resize(_frame_manager, *_device, *_swapchain);
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
    _device = new Device(*_physical_device);
    spdlog::info("[engine] device created.");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device().vkdevice());

    // 内存分配工具
    init_vma();


    create_descriptor_pool();


    // 创建 swapchain
    _swapchain = new Swapchain(*_device, *_window, _surface);


    _frame_manager = new Hiss::FrameManager(*_device, *_swapchain);

    _shader_loader = new ShaderLoader(*_device);
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
    DELETE(_shader_loader);
    DELETE(_frame_manager);
    DELETE(_swapchain);

    // 销毁 vma 的分配器
    vmaDestroyAllocator(allocator);

    vkdevice().destroy(descriptor_pool._value);

    DELETE(_device);
    DELETE(_physical_device);
    _instance->vkinstance().destroy(_surface);
    _instance->vkinstance().destroy(_debug_messenger);
    DELETE(_instance);
    DELETE(_window);
}


void Hiss::Engine::preupdate() noexcept
{
    _frame_manager->acquire_frame();
}


void Hiss::Engine::postupdate() noexcept
{
    _frame_manager->submit_frame();
}


Hiss::Image2D* Hiss::Engine::create_depth_image() const
{
    return new Hiss::Image2D(allocator, *_device,
                             Hiss::Image2D::Info{
                                     .format       = depth_format(),
                                     .extent       = extent(),
                                     .usage        = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                     .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                     .aspect       = vk::ImageAspectFlagBits::eDepth,
                                     .init_layout  = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                             });
}


void Hiss::Engine::create_descriptor_pool()
{
    descriptor_pool._value = vkdevice().createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .maxSets       = descriptor_set_max_number,
            .poolSizeCount = static_cast<uint32_t>(pool_size.size()),
            .pPoolSizes    = pool_size.data(),
    });

    spdlog::info("[descriptor pool] descriptor set max number: {}", descriptor_set_max_number);
    for (auto& item: pool_size)
        spdlog::info("[descriptor pool] max number: ({}, {})", to_string(item.type), item.descriptorCount);
}

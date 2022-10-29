#include "engine.hpp"
#include "utils/tools.hpp"
#include "vk_config.hpp"
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
    _frame_manager = Hiss::FrameManager::on_resize(_frame_manager, *_device, *_swapchain);
}


void Hiss::Engine::prepare()
{
    _window = new Window(name(), WINDOW_WIDTH, WINDOW_HEIGHT);

    timer._value.start();

    // 创建 vulkan 应用的 instance
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    _instance = new Instance(name(), &_debug_utils_messenger_info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance->vkinstance());


    // 创建 validation 的 debug messenger
    _debug_messenger = _instance->vkinstance().createDebugUtilsMessengerEXT(_debug_utils_messenger_info);


    // 创建 surface
    _surface = _window->create_surface(_instance->vkinstance());


    // 创建 physical device
    auto physical_device = _instance->gpu_pick(_surface);
    if (!physical_device.has_value())
        throw std::runtime_error("no suitable gpu found.");
    _physical_device = new GPU(physical_device.value(), _surface);


    // 创建 logical device
    _device = new Device(*_physical_device);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device().vkdevice());

    // 内存分配工具
    init_vma();


    create_descriptor_pool();


    // 创建 swapchain
    _swapchain = new Swapchain(*_device, *_window, _surface);


    _frame_manager = new Hiss::FrameManager(*_device, *_swapchain);

    _shader_loader = new ShaderLoader(*_device);


    // 创建默认的纹理
    default_texture = std::make_unique<Hiss::Texture>(*_device, allocator, texture / "awesomeface.jpg",
                                                      vk::Format::eR8G8B8A8Srgb);


    create_material_descriptor_layout();
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
    // 销毁默认的纹理
    default_texture.reset();

    _device->vkdevice().destroy(material_layout);

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
    timer._value.tick();
    _frame_manager->acquire_frame();
}


void Hiss::Engine::postupdate() noexcept
{
    _frame_manager->submit_frame();
}


Hiss::Image2D* Hiss::Engine::create_depth_attach(vk::SampleCountFlagBits sample) const
{
    return new Hiss::Image2D(
            allocator, *_device,
            Hiss::Image2DCreateInfo{
                    .format       = depth_format(),
                    .extent       = extent(),
                    .usage        = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                    .samples      = sample,
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


void Hiss::Engine::depth_attach_execution_barrier(vk::CommandBuffer command_buffer, Hiss::Image2D& image)
{
    image.execution_barrier(
            command_buffer, {vk::PipelineStageFlagBits::eLateFragmentTests},
            {vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::AccessFlagBits::eDepthStencilAttachmentWrite});
}


void Hiss::Engine::color_attach_layout_trans_1(vk::CommandBuffer command_buffer, Hiss::Image2D& image)
{
    image.transfer_layout(
            command_buffer, {vk::PipelineStageFlagBits::eTopOfPipe, vk::AccessFlags()},
            {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
            vk::ImageLayout::eColorAttachmentOptimal, true);
}


void Hiss::Engine::color_attach_layout_trans_2(vk::CommandBuffer command_buffer, Hiss::Image2D& image)
{
    image.transfer_layout(
            command_buffer,
            {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
            {vk::PipelineStageFlagBits::eBottomOfPipe, {}}, vk::ImageLayout::ePresentSrcKHR);
}


Hiss::Image2D* Hiss::Engine::create_color_attach(vk::SampleCountFlagBits sample) const
{
    return new Hiss::Image2D(allocator, *_device,
                             Hiss::Image2DCreateInfo{
                                     .format       = color_format(),
                                     .extent       = extent(),
                                     .usage        = vk::ImageUsageFlagBits::eColorAttachment,
                                     .samples      = sample,
                                     .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                     .aspect       = vk::ImageAspectFlagBits::eColor,
                                     .init_layout  = vk::ImageLayout::eColorAttachmentOptimal,
                             });
}


vk::Viewport Hiss::Engine::viewport() const
{
    return vk::Viewport{
            .x        = 0.f,
            .y        = 0.f,
            .width    = (float) this->_swapchain->present_extent().width,
            .height   = (float) this->_swapchain->present_extent().height,
            .minDepth = 0.f,
            .maxDepth = 1.f,
    };
}


vk::Rect2D Hiss::Engine::scissor() const
{
    return vk::Rect2D{
            .offset = {0, 0},
            .extent = this->_swapchain->present_extent(),
    };
}


vk::DescriptorSet Hiss::Engine::create_descriptor_set(vk::DescriptorSetLayout layout, const std::string& debug_name)
{
    auto descriptor_set = _device->vkdevice()
                                  .allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                                          .descriptorPool     = descriptor_pool._value,
                                          .descriptorSetCount = 1,
                                          .pSetLayouts        = &layout,
                                  })
                                  .front();

    if (!debug_name.empty())
    {
        _device->set_debug_name(vk::ObjectType::eDescriptorSet, (VkDescriptorSet) descriptor_set, debug_name);
    }

    return descriptor_set;
}

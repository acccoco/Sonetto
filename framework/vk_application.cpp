#include "vk_application.hpp"
#include "utils/tools.hpp"
#include "vk/vk_config.hpp"


/**
 * vulkan 相关函数的 dynamic loader
 * 这是变量定义的位置，vulkan.hpp 会通过 extern 引用这个全局变量
 * 需要初始化两次：
 *   第一次传入 vkGetInstanceProcAddr
 *   第二次传入 instance
 */
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE


void Hiss::VkApplication::resize()
{
    Application::resize();
    _logger->info("[ExampleBase] resize");


    _device->vkdevice().waitIdle();


    /* destroy */
    DELETE(_swapchain);
    for (auto framebuffer: _framebuffers)
        _device->vkdevice().destroy(framebuffer);
    _framebuffers.clear();
    DELETE(_depth_image_view);
    DELETE(_depth_image);


    /* create */
    _swapchain = new Swapchain(*_device, *_window, _surface);
    init_depth_buffer();
    init_framebuffer();
    _frame_manager = Hiss::FrameManager::resize(_frame_manager, *_device, *_swapchain);
}


/**
 * 创建最简单的 renderpass，只包含 1 subpass，1 color attachment，1 depth attachment
 */
void Hiss::VkApplication::init_render_pass()
{
    std::array<vk::AttachmentDescription, 2> attachments = {
            /* color attachment */
            vk::AttachmentDescription{
                    .format        = _swapchain->get_color_format(),
                    .samples       = vk::SampleCountFlagBits::e1,
                    .loadOp        = vk::AttachmentLoadOp::eClear,
                    .storeOp       = vk::AttachmentStoreOp::eStore,
                    .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,    // 由外部负责 layout trans
                    .finalLayout   = vk::ImageLayout::eColorAttachmentOptimal,
            },
            /* depth attachment */
            vk::AttachmentDescription{
                    .format        = _depth_format,
                    .samples       = vk::SampleCountFlagBits::e1,
                    .loadOp        = vk::AttachmentLoadOp::eClear,
                    .storeOp       = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    .finalLayout   = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            },
    };


    vk::AttachmentReference color_attach_ref = {0, vk::ImageLayout::eColorAttachmentOptimal};
    vk::AttachmentReference depth_attach_ref = {1, vk::ImageLayout::eDepthStencilAttachmentOptimal};


    vk::SubpassDescription subpass = {
            .pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount    = 1,
            .pColorAttachments       = &color_attach_ref,
            .pDepthStencilAttachment = &depth_attach_ref,
    };


    _simple_render_pass = _device->vkdevice().createRenderPass(vk::RenderPassCreateInfo{
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments    = attachments.data(),
            .subpassCount    = 1,
            .pSubpasses      = &subpass,
            .dependencyCount = 0,    // 由外部的 memory barrier 负责 layout transition
            .pDependencies   = nullptr,
    });
}


void Hiss::VkApplication::init_depth_buffer()
{
    _depth_format = _device->gpu_get().depth_stencil_format.get();
    _depth_image  = new Image(Hiss::Image::CreateInfo{
             .device            = *_device,
             .format            = _depth_format,
             .extent            = _swapchain->get_extent(),
             .usage             = vk::ImageUsageFlagBits::eDepthStencilAttachment,
             .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
    });

    _device->set_debug_name(vk::ObjectType::eImage, (VkImage) _depth_image->vkimage(), "default depth image");

    _depth_image->layout_tran(vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
                              vk::ImageAspectFlagBits::eDepth, 0, 1);
    _depth_image_view = new ImageView(*_depth_image, vk::ImageAspectFlagBits::eDepth, 0, 1);
}


void Hiss::VkApplication::prepare()
{
    Application::prepare();
    _logger->info("[ExampleBase] prepare");


    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    _debug_user_data._logger = _validation_logger;
    _instance                = new Instance(Application::app_name(), &_debug_utils_messenger_info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance->handle_get());
    _logger->info("[instance] instance created.");


    _debug_messenger = _instance->handle_get().createDebugUtilsMessengerEXT(_debug_utils_messenger_info);


    _surface = _window->create_surface(_instance->handle_get());


    auto physical_device = _instance->gpu_pick(_surface);
    if (!physical_device.has_value())
        throw std::runtime_error("no suitable gpu found.");
    _physical_device = new GPU(physical_device.value(), _surface);
    _logger->info("[physical device] physical device created.");


    _device = new Device(*_physical_device, *_logger);
    _logger->info("[device] device created.");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_device->vkdevice());


    _swapchain = new Swapchain(*_device, *_window, _surface);
    _logger->info("[swapchain] image count: {}", _swapchain->get_image_number());

    _frame_manager = new Hiss::FrameManager(*_device, *_swapchain);


    for (size_t i = 0; i < _frames.size(); ++i)
    {
        _frames[i] = new Frame(*_device, std::to_string(i));
    }
    _logger->info("[render context] frames in-flight: {}", 2);

    _shader_loader = new ShaderLoader(*_device);


    init_depth_buffer();
    init_render_pass();
    init_framebuffer();
}


void Hiss::VkApplication::init_framebuffer()
{
    _framebuffers.reserve(_swapchain->get_image_number());
    for (uint32_t index = 0; index < _swapchain->get_image_number(); ++index)
    {
        _framebuffers.push_back(_device->create_framebuffer(_simple_render_pass,
                                                            {
                                                                    _swapchain->get_image_view(index),
                                                                    _depth_image_view->view_get(),
                                                            },
                                                            _swapchain->get_extent()));
    }
}


void Hiss::VkApplication::clean()
{
    _logger->info("[ExampleBase] clean");

    DELETE(_shader_loader);

    for (auto framebuffer: _framebuffers)
        _device->vkdevice().destroy(framebuffer);
    _framebuffers.clear();
    DELETE(_depth_image_view);
    DELETE(_depth_image);
    for (auto& frame: _frames)
    {
        DELETE(frame);
    }
    _device->vkdevice().destroy(_simple_render_pass);
    DELETE(_frame_manager);
    DELETE(_swapchain);
    DELETE(_device);
    DELETE(_physical_device);
    _instance->handle_get().destroy(_surface);
    _instance->handle_get().destroy(_debug_messenger);
    DELETE(_instance);

    Application::clean();
}


void Hiss::VkApplication::update(double delte_time) noexcept
{
    Application::update(0);
}


vk::Bool32 Hiss::VkApplication::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                             VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                             const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    const char* type;
    switch (static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(message_type))
    {
        case vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral: type = "General"; break;
        case vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation: type = "Validation"; break;
        case vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance: type = "Performance"; break;
        default: type = "?";
    }

    auto logger = reinterpret_cast<DebugUserData*>(user_data)->_logger;
    switch (message_severity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            logger->warn("[{}]: {}\n", type, callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            logger->error("[{}]: {}\n", type, callback_data->pMessage);
            break;
        default: logger->info("[{}]: {}", type, callback_data->pMessage);
    }

    return VK_FALSE;
}


void Hiss::VkApplication::next_frame()
{
    _current_frame_index = (_current_frame_index + 1) % IN_FLIGHT_CNT;
}


Hiss::Frame& Hiss::VkApplication::current_frame()
{
    return *_frames[_current_frame_index];
}


void Hiss::VkApplication::prepare_frame()
{
    /* wait fence */
    if (!current_frame().fence_get().empty())
    {
        (void) _device->vkdevice().waitForFences(current_frame().fence_get(), VK_TRUE, UINT64_MAX);
        _device->fence_pool().revert(current_frame().fence_pop_all());
    }


    /* acquire iamge from swapchian */
    Hiss::EnumRecreate need_recreate;
    std::tie(need_recreate, _swapchain_image_index) =
            _swapchain->acquire_image(current_frame().semaphore_swapchain_acquire());

    if (need_recreate == Hiss::EnumRecreate::NEED)
    {
        _logger->info("resize: acquire");
        resize();
        return;
    }
}


void Hiss::VkApplication::submit_frame()
{
    vk::Fence          fence         = _device->fence_pool().acquire(true);
    Hiss::EnumRecreate need_recreate = _swapchain->submit_image(
            _swapchain_image_index, current_frame().semaphore_render_complete(),
            current_frame().semaphore_transfer_done(), fence, current_frame().command_buffer_present());
    current_frame().fence_push(fence);

    if (need_recreate == Hiss::EnumRecreate::NEED)
    {
        _logger->info("resize: submit");
        resize();
    }
}


std::array<vk::CommandBuffer, Hiss::VkApplication::IN_FLIGHT_CNT> Hiss::VkApplication::compute_command_buffer()
{
    std::array<vk::CommandBuffer, IN_FLIGHT_CNT> buffers;
    for (uint32_t i = 0; i < IN_FLIGHT_CNT; ++i)
    {
        buffers[i] = _frames[i]->command_buffer_compute();
    }
    return buffers;
}
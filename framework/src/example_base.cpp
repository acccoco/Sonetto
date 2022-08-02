#include "example_base.hpp"
#include "tools.hpp"


/**
 * vulkan 相关函数的 dynamic loader
 * 这是变量定义的位置，vulkan.hpp 会通过 extern 引用这个全局变量
 * 需要初始化两次：
 *   第一次传入 vkGetInstanceProcAddr
 *   第二次传入 instance
 */
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE


vk::PipelineShaderStageCreateInfo Hiss::ExampleBase::shader_load(const std::string& file, vk::ShaderStageFlagBits stage)
{
    std::vector<char>          code = read_file(file);
    vk::ShaderModuleCreateInfo info = {
            .codeSize = code.size(),    // 单位是字节

            /* vector 容器可以保证转换为 uint32 后仍然是对齐的 */
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    };
    vk::ShaderModule shader_module = _device->vkdevice().createShaderModule(info);
    shader_modules.push_back(shader_module);

    return vk::PipelineShaderStageCreateInfo{
            .stage  = stage,
            .module = shader_module,
            .pName  = "main",
    };
}


void Hiss::ExampleBase::instance_prepare(const std::string& app_name)
{
    /* 找到所需的 extensions */
    std::vector<const char*> extensions = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,                // 可以将 validation 信息打印出来
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,    // 基于 metal API 的 vulkan 实现需要这些扩展
    };
    auto glfw_extension = _window->extensions_get();
    extensions.insert(extensions.end(), glfw_extension.begin(), glfw_extension.end());


    /* 所需的 layers */
    std::vector<const char*> layers = {
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
    if (debug)
        layers.push_back("VK_LAYER_KHRONOS_validation");
    if (!Hiss::instance_layers_check(layers))
        throw std::runtime_error("layers unsupported.");


    _instance = new Instance(app_name, extensions, layers, &_debug_utils_messenger_info);
}


void Hiss::ExampleBase::resize()
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
    depth_buffer_prepare();
    framebuffer_prepare();
}


/**
 * 创建最简单的 renderpass，只包含 1 subpass，1 color attachment，1 depth attachment
 */
void Hiss::ExampleBase::render_pass_perprare()
{
    std::array<vk::AttachmentDescription, 2> attachments = {
            /* color attachment */
            vk::AttachmentDescription{
                    .format        = _swapchain->color_format(),
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


void Hiss::ExampleBase::depth_buffer_prepare()
{
    auto format = _device->gpu_get().format_filter(
            {
                    vk::Format::eD32Sfloat,
                    vk::Format::eD32SfloatS8Uint,
                    vk::Format::eD24UnormS8Uint,
            },
            vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    if (!format.has_value())
        throw std::runtime_error("no suitalbe depth format.");

    _depth_format = format.value();
    _depth_image  = new Image(Hiss::Image::ImageCreateInfo{
             .device            = *_device,
             .format            = _depth_format,
             .extent            = _swapchain->extent_get(),
             .usage             = vk::ImageUsageFlagBits::eDepthStencilAttachment,
             .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
    });
    _depth_image->layout_tran(vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
                              vk::ImageAspectFlagBits::eDepth, 0, 1);
    _depth_image_view = new ImageView(*_depth_image, vk::ImageAspectFlagBits::eDepth, 0, 1);
}


void Hiss::ExampleBase::prepare()
{
    Application::prepare();
    _logger->info("[ExampleBase] prepare");


    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    _debug_user_data._logger = _validation_logger;
    instance_prepare(Application::app_name_get());
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance->handle_get());
    _logger->info("[instance] instance created.");


    _debug_messenger = _instance->handle_get().createDebugUtilsMessengerEXT(_debug_utils_messenger_info);


    _surface = _window->surface_create(_instance->handle_get());


    auto physical_device = _instance->gpu_pick(_surface);
    if (!physical_device.has_value())
        throw std::runtime_error("no suitable gpu found.");
    _physical_device = new GPU(_instance->gpu_pick(_surface).value(), _surface);
    _logger->info("[physical device] physical device created.");


    _device = new Device(*_physical_device, *_logger);
    _logger->info("[device] device created.");


    _swapchain = new Swapchain(*_device, *_window, _surface);
    _logger->info("[swapchain] image count: {}", _swapchain->images_count());


    _render_context = new RenderContext(*_device, IN_FLIGHT_CNT);
    _logger->info("[render context] frames in-flight: {}", 2);


    depth_buffer_prepare();
    render_pass_perprare();
    framebuffer_prepare();
}


void Hiss::ExampleBase::framebuffer_prepare()
{
    std::array<vk::ImageView, 2> attachments;
    attachments[1] = _depth_image_view->view_get();

    vk::FramebufferCreateInfo framebuffer_info = {
            .renderPass      = _simple_render_pass,
            .attachmentCount = 2,    // static_cast<uint32_t>(attachments.size()),
            .pAttachments    = attachments.data(),
            .width           = _swapchain->extent_get().width,
            .height          = _swapchain->extent_get().height,
            .layers          = 1,
    };

    _framebuffers.reserve(_swapchain->images_count());
    for (uint32_t index = 0; index < _swapchain->images_count(); ++index)
    {
        attachments[0] = _swapchain->image_view_get(index);
        _framebuffers.push_back(_device->vkdevice().createFramebuffer(framebuffer_info));
    }
}


void Hiss::ExampleBase::clean()
{
    _logger->info("[ExampleBase] clean");

    for (auto shader_module: shader_modules)
        _device->vkdevice().destroy(shader_module);

    for (auto framebuffer: _framebuffers)
        _device->vkdevice().destroy(framebuffer);
    _framebuffers.clear();
    DELETE(_depth_image_view);
    DELETE(_depth_image);
    DELETE(_render_context);
    _device->vkdevice().destroy(_simple_render_pass);
    DELETE(_swapchain);
    DELETE(_device);
    DELETE(_physical_device);
    _instance->handle_get().destroy(_surface);
    _instance->handle_get().destroy(_debug_messenger);
    DELETE(_instance);

    Application::clean();
}


void Hiss::ExampleBase::update() noexcept
{
    Application::update();
}


vk::Bool32 Hiss::ExampleBase::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
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
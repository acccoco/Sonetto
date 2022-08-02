#pragma once
#include <iostream>
#include "instance.hpp"
#include "device.hpp"
#include "swapchain.hpp"
#include "application.hpp"
#include "render_context.hpp"


namespace Hiss
{
struct DebugUserData
{
    std::shared_ptr<spdlog::logger> _logger;
};


class ExampleBase : public Application
{
public:
    explicit ExampleBase(const std::string& app_name)
        : Application(app_name)
    {}
    ~ExampleBase() override = default;


protected:
    void prepare() override;
    void resize() override;
    void update() noexcept override;
    void clean() override;
    void wait_idle() final { _device->vkdevice().waitIdle(); }

    vk::PipelineShaderStageCreateInfo shader_load(const std::string& file, vk::ShaderStageFlagBits stage);
    [[nodiscard]] vk::Format          depth_format_get() const { return _depth_format; }


private:
    void instance_prepare(const std::string& app_name);
    void depth_buffer_prepare();
    void render_pass_perprare();
    void framebuffer_prepare();

    static vk::Bool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                     VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                     const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);


    // members =======================================================


protected:
    static constexpr uint32_t IN_FLIGHT_CNT = 2;    // number of frames in-flight


    vk::Format _depth_format     = {};
    Image*     _depth_image      = nullptr;
    ImageView* _depth_image_view = nullptr;

    Instance*                    _instance           = nullptr;
    vk::SurfaceKHR               _surface            = nullptr;
    GPU*                         _physical_device    = nullptr;
    Hiss::Device*                _device             = nullptr;
    Swapchain*                   _swapchain          = nullptr;
    vk::RenderPass               _simple_render_pass = VK_NULL_HANDLE;
    RenderContext*               _render_context     = nullptr;
    std::vector<vk::Framebuffer> _framebuffers = {};    // (#framebuffer) == (#swapchian images) != (# frames in-flight)
    std::vector<vk::ShaderModule> shader_modules{};


private:
    DebugUserData              _debug_user_data = {};
    vk::DebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;

    const vk::DebugUtilsMessengerCreateInfoEXT _debug_utils_messenger_info = {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debug_callback,
            .pUserData       = &_debug_user_data,
    };
};
}    // namespace Hiss
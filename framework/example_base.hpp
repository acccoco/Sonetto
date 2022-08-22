#pragma once
#include <iostream>
#include "frame.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "swapchain.hpp"
#include "application.hpp"


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

    static constexpr uint32_t IN_FLIGHT_CNT = 2;    // number of frames in-flight

protected:
    void prepare() override;
    void resize() override;
    void update(double delte_time) noexcept override;
    void clean() override;
    void wait_idle() final { _device->vkdevice().waitIdle(); }


    /**
     * 使用这个方法来统一管理 shader 资源
     */
    vk::PipelineShaderStageCreateInfo shader_load(const std::string& file, vk::ShaderStageFlagBits stage);


    /**
     * 当前的 depth format
     */
    [[nodiscard]] vk::Format depth_format_get() const { return _depth_format; }

    /**
     * 获取当前 frame 的资源：command buffer，同步原语
     */
    Frame& current_frame();

    /**
     * 进入下一帧
     */
    void next_frame() final;

    /**
     * 当前 frame 在所有的 frames-in-flight 中的 index
     */
    [[nodiscard]] uint32_t current_frame_index() const { return _current_frame_index; }

    [[nodiscard]] uint32_t current_swapchain_image_index() const { return _swapchain_image_index; }


    void frame_prepare();

    void frame_submit();


    std::array<vk::CommandBuffer, IN_FLIGHT_CNT> command_buffers_compute();


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
    vk::Format _depth_format     = {};
    Image*     _depth_image      = nullptr;
    ImageView* _depth_image_view = nullptr;

    Instance*                    _instance           = nullptr;
    vk::SurfaceKHR               _surface            = nullptr;
    GPU*                         _physical_device    = nullptr;
    Hiss::Device*                _device             = nullptr;
    Swapchain*                   _swapchain          = nullptr;
    vk::RenderPass               _simple_render_pass = VK_NULL_HANDLE;
    std::vector<vk::Framebuffer> _framebuffers = {};    // (#framebuffer) == (#swapchian images) != (# frames in-flight)
    std::vector<vk::ShaderModule> shader_modules{};


private:
    DebugUserData                              _debug_user_data            = {};
    vk::DebugUtilsMessengerEXT                 _debug_messenger            = VK_NULL_HANDLE;
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


    uint32_t                          _current_frame_index   = 0;
    uint32_t                          _swapchain_image_index = 0;
    std::array<Frame*, IN_FLIGHT_CNT> _frames                = {};
};
}    // namespace Hiss
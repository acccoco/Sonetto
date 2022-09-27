#pragma once
#include <iostream>
#include "frame.hpp"
#include "utils/shader_loader.hpp"
#include "vk/device.hpp"
#include "vk/instance.hpp"
#include "vk/swapchain.hpp"
#include "application.hpp"
#include "frame_manager.hpp"


namespace Hiss
{
struct DebugUserData
{
    std::shared_ptr<spdlog::logger> _logger;
};


class VkApplication : public Application
{
public:
    explicit VkApplication(const std::string& app_name)
        : Application(app_name)
    {}
    ~VkApplication() override = default;

    static constexpr uint32_t IN_FLIGHT_CNT = 2;    // number of frames in-flight

protected:
    void prepare() override;
    void resize() override;
    void update(double delte_time) noexcept override;
    void clean() override;
    void wait_idle() final
    {
        _device->vkdevice().waitIdle();
    }


    /// 当前的 depth format
    [[nodiscard]] vk::Format get_depth_format() const
    {
        return _depth_format;
    }

    /// 获取当前 frame 的资源：command buffer，同步原语
    Frame& current_frame();

    /// 进入下一帧
    void next_frame() final;

    /// 当前 frame 在所有的 frames-in-flight 中的 index
    [[nodiscard]] uint32_t current_frame_index() const
    {
        return _current_frame_index;
    }

    [[nodiscard]] uint32_t current_swapchain_image_index() const
    {
        return _swapchain_image_index;
    }

    [[nodiscard]] vk::Extent2D get_extent() const
    {
        return this->_swapchain->get_extent();
    }

    [[nodiscard]] vk::Format get_color_format() const {
        return this->_swapchain->get_color_format();
    }

    void prepare_frame();

    void submit_frame();

    const Device& get_device()
    {
        return *_device;
    }


    std::array<vk::CommandBuffer, IN_FLIGHT_CNT> compute_command_buffer();


private:
    void init_depth_buffer();
    void init_render_pass();
    void init_framebuffer();

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
    ShaderLoader*                _shader_loader = nullptr;

    Hiss::FrameManager* _frame_manager = nullptr;


private:
    DebugUserData              _debug_user_data = {};
    vk::DebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;

    const vk::DebugUtilsMessengerCreateInfoEXT _debug_utils_messenger_info = {
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                         // | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            .pfnUserCallback = debug_callback,
            .pUserData       = &_debug_user_data,
    };


    uint32_t                          _current_frame_index   = 0;
    uint32_t                          _swapchain_image_index = 0;
    std::array<Frame*, IN_FLIGHT_CNT> _frames                = {};
};
}    // namespace Hiss
#pragma once
#include "utils/shader_loader.hpp"
#include "utils/timer.hpp"
#include "vk/device.hpp"
#include "vk/instance.hpp"
#include "vk/swapchain.hpp"
#include "vk/vk_common.hpp"
#include "frame.hpp"


namespace Hiss
{
class Application
{
public:
    explicit Application(const std::string& app_name)
        : name(app_name)
    {}
    ~Application() = default;

    // 特殊接口 ==========================================

    void prepare();
    void resize();
    void perupdate() noexcept;
    void postupdate() noexcept;
    void clean();
    void wait_idle() const
    {
        device().vkdevice().waitIdle();
    }

    [[nodiscard]] bool should_close() const
    {
        return _window->should_close();
    }

    [[nodiscard]] bool should_resize() const
    {
        return _window->has_resized();
    }

    // 各种属性 ============================================


    [[nodiscard]] vk::Extent2D get_extent() const
    {
        return this->_swapchain->present_extent.get();
    }


    [[nodiscard]] vk::Format get_color_format() const
    {
        return this->_swapchain->get_color_format();
    }


private:
    void init_vma();


    // members =======================================================

public:
    Prop<vk::Format, Application>  depth_format{};
    Prop<std::string, Application> name{};
    Prop<Timer, Application>       timer{};

    PropPtr<Device, Application>       device{nullptr};
    PropPtr<FrameManager, Application> frame_manager{nullptr};
    PropPtr<ShaderLoader, Application> shader_loader{nullptr};
    PropPtr<Frame, Application>        current_frame{nullptr};


    VmaAllocator allocator{};


protected:
    Instance*      _instance        = nullptr;
    vk::SurfaceKHR _surface         = nullptr;
    GPU*           _physical_device = nullptr;
    Swapchain*     _swapchain       = nullptr;


private:
    vk::DebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;

    Window* _window = nullptr;
};
}    // namespace Hiss
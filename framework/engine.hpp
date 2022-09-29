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
class Engine
{
public:
    explicit Engine(const std::string& app_name)
        : name(app_name)
    {}
    ~Engine() = default;

    // 特殊接口 ==========================================

    void prepare();
    void resize();
    void preupdate() noexcept;
    void postupdate() noexcept;
    void clean();
    void wait_idle() const { device().vkdevice().waitIdle(); }
    void poll_event() { _window->poll_event(); }

    [[nodiscard]] bool should_close() const { return _window->should_close(); }

    [[nodiscard]] bool should_resize() const { return _window->has_resized(); }


    // 常用的工具 =========================================


    // 创建符合 swapchain 大小的 image，需要应用自己管理内存
    [[nodiscard]] Image2D* create_depth_image() const;


private:
    void init_vma();


    // members =======================================================

public:
    Prop<std::string, Engine> name{};
    Prop<Timer, Engine>       timer{};

    PropPtr<Device, Engine>       device{nullptr};
    PropPtr<FrameManager, Engine> frame_manager{nullptr};
    Prop<ShaderLoader*, Engine>   shader_loader{nullptr};


    [[nodiscard]] vk::Extent2D extent() const { return this->_swapchain->present_extent(); }
    [[nodiscard]] vk::Format   color_format() const { return this->_swapchain->color_format(); }
    [[nodiscard]] vk::Device   vkdevice() const { return this->device().vkdevice(); }
    [[nodiscard]] vk::Format   depth_format() const { return this->device._ptr->gpu().depth_stencil_format(); }
    [[nodiscard]] Frame&       current_frame() const { return frame_manager._ptr->current_frame(); }


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
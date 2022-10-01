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


    [[nodiscard]] vk::Extent2D  extent() const { return this->_swapchain->present_extent(); }
    [[nodiscard]] vk::Format    color_format() const { return this->_swapchain->color_format(); }
    [[nodiscard]] vk::Device    vkdevice() const { return this->device().vkdevice(); }
    [[nodiscard]] vk::Format    depth_format() const { return this->_device->gpu().depth_stencil_format(); }
    [[nodiscard]] Frame&        current_frame() const { return _frame_manager->current_frame(); }
    [[nodiscard]] Device&       device() const { return *_device; }
    [[nodiscard]] FrameManager& frame_manager() const { return *_frame_manager; }
    [[nodiscard]] ShaderLoader& shader_loader() const { return *_shader_loader; }


    VmaAllocator allocator{};


private:
    Instance*      _instance        = nullptr;
    vk::SurfaceKHR _surface         = nullptr;
    GPU*           _physical_device = nullptr;
    Swapchain*     _swapchain       = nullptr;
    Window*        _window          = nullptr;
    Device*        _device          = nullptr;
    FrameManager*  _frame_manager   = nullptr;
    ShaderLoader*  _shader_loader   = nullptr;


    vk::DebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;
};
}    // namespace Hiss
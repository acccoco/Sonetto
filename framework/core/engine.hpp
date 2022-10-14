#pragma once
#include "utils/shader_loader.hpp"
#include "utils/timer.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "swapchain.hpp"
#include "vk_common.hpp"
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


#pragma region public 特殊接口
public:
    void prepare();
    void resize();
    void preupdate() noexcept;
    void postupdate() noexcept;
    void clean();
    void wait_idle() const { device().vkdevice().waitIdle(); }
    void poll_event() { _window->poll_event(); }

    bool should_close() const { return _window->should_close(); }

    bool should_resize() const { return _window->has_resized(); }

#pragma endregion


#pragma region private 工具方法

public:
    // 创建符合 swapchain 大小的 image，需要应用自己管理内存
    Image2D* create_depth_attach(vk::SampleCountFlagBits sample = vk::SampleCountFlagBits::e1) const;

    Image2D* create_color_attach(vk::SampleCountFlagBits sample = vk::SampleCountFlagBits::e1) const;

    static void depth_attach_execution_barrier(vk::CommandBuffer command_buffer, Image2D& image);

    // layout 转换为 colorAttachment，不保留之前的数据
    static void color_attach_layout_trans_1(vk::CommandBuffer command_buffer, Image2D& image);

    // layout 转换为 present，保留之前的数据，最后一个 stage 是 color attachment
    static void color_attach_layout_trans_2(vk::CommandBuffer command_buffer, Image2D& image);

    vk::DescriptorSet create_descriptor_set(vk::DescriptorSetLayout layout);

#pragma endregion


#pragma region private method 初始化
private:
    void init_vma();

    void create_descriptor_pool();

#pragma endregion


#pragma region public 属性
public:
    Prop<std::string, Engine> name{};
    Prop<Timer, Engine>       timer{};


    vk::Device vkdevice() const { return this->device().vkdevice(); }
    Device&    device() const { return *_device; }
    GPU&       gpu() const { return _device->gpu(); }
    Queue&     queue() const { return _device->queue(); }

    vk::Extent2D extent() const { return this->_swapchain->present_extent(); }
    vk::Viewport viewport() const;
    vk::Rect2D   scissor() const;

    vk::Format color_format() const { return this->_swapchain->color_format(); }
    vk::Format depth_format() const { return this->_device->gpu().depth_stencil_format(); }

    // 当前帧，和在 frame manager 中获得的是一样的
    Frame&        current_frame() const { return _frame_manager->current_frame(); }
    FrameManager& frame_manager() const { return *_frame_manager; }

    ShaderLoader& shader_loader() const { return *_shader_loader; }


    VmaAllocator                     allocator = {};
    Prop<vk::DescriptorPool, Engine> descriptor_pool{VK_NULL_HANDLE};

#pragma endregion


#pragma region private 成员字段
private:
    Instance*     _instance        = nullptr;
    GPU*          _physical_device = nullptr;
    Swapchain*    _swapchain       = nullptr;
    Window*       _window          = nullptr;
    Device*       _device          = nullptr;
    FrameManager* _frame_manager   = nullptr;
    ShaderLoader* _shader_loader   = nullptr;

    vk::SurfaceKHR             _surface         = VK_NULL_HANDLE;
    vk::DebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;
#pragma endregion
};
}    // namespace Hiss
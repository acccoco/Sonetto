#pragma once
#include <memory>
#include "utils/shader_loader.hpp"
#include "utils/timer.hpp"
#include "core/device.hpp"
#include "core/instance.hpp"
#include "swapchain.hpp"
#include "core/vk_common.hpp"
#include "frame.hpp"
#include "frame_manager.hpp"
#include "texture.hpp"
#include "utils/vk_func.hpp"


// TODO
//  像 descriptor 这样，多分几个类。
//  拆分 engine 这个资源；；最好将 material descriptor 放到公共资源那里去，很危险。
//  整理一下 mesh model 这个类
//  再整理一下 device 的类，拆分到 vulkan core 中
//  light ，frame，scene都可以抽象出辅助函数
//  沿着这个思路继续，看看还能够抽象什么东西


namespace Hiss
{
class Engine
{
public:
    explicit Engine(const std::string& app_name)
        : name(app_name)
    {}
    ~Engine() = default;


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




public:
    // 创建符合 swapchain 大小的 image，需要应用自己管理内存
    Image2D* create_depth_attach(vk::SampleCountFlagBits sample = vk::SampleCountFlagBits::e1) const;

    Image2D* create_color_attach(vk::SampleCountFlagBits sample = vk::SampleCountFlagBits::e1) const;

    static void depth_attach_execution_barrier(vk::CommandBuffer command_buffer, Image2D& image);

    // layout 转换为 colorAttachment，不保留之前的数据
    static void color_attach_layout_trans_1(vk::CommandBuffer command_buffer, Image2D& image);

    // layout 转换为 present，保留之前的数据，最后一个 stage 是 color attachment
    static void color_attach_layout_trans_2(vk::CommandBuffer command_buffer, Image2D& image);

    vk::DescriptorSet create_descriptor_set(vk::DescriptorSetLayout layout, const std::string& debug_name = "");



private:
    void init_vma();

    void create_descriptor_pool();


    void create_material_descriptor_layout()
    {
        material_layout = Hiss::Initial::descriptor_set_layout(
                _device->vkdevice(),
                std::vector<Hiss::Initial::BindingInfo>{
                        {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment},
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                });
    }



public:
    Prop<std::string, Engine> name{};
    Prop<Timer, Engine>       timer{};

    // 默认的，用于占位的纹理
    std::unique_ptr<Texture> default_texture;

    vk::DescriptorSetLayout material_layout;


    vk::Device vkdevice() const { return this->device().vkdevice(); }
    Device&    device() const { return *_device; }
    GPU&       gpu() const { return _device->gpu(); }
    Queue&     queue() const { return _device->queue(); }

    vk::Extent2D extent() const { return this->_swapchain->present_extent(); }
    vk::Viewport viewport() const;
    vk::Rect2D   scissor() const;

    // 画面的长宽比
    float aspect() const
    {
        return (float) _swapchain->present_extent().width / (float) _swapchain->present_extent().height;
    }

    vk::Format color_format() const { return this->_swapchain->color_format(); }
    vk::Format depth_format() const { return this->_device->gpu().depth_stencil_format(); }

    // 当前帧，和在 frame manager 中获得的是一样的
    Frame&        current_frame() const { return _frame_manager->current_frame(); }
    FrameManager& frame_manager() const { return *_frame_manager; }

    ShaderLoader& shader_loader() const { return *_shader_loader; }


    VmaAllocator                     allocator = {};
    Prop<vk::DescriptorPool, Engine> descriptor_pool{VK_NULL_HANDLE};



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
};
}    // namespace Hiss
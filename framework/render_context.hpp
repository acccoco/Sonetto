#pragma once
#include "vk_common.hpp"
#include "device.hpp"


namespace Hiss
{

/**
 * 渲染所需的 command buffer，同步原语等，数量等于 frames inflight
 */
class RenderContext
{
public:
    RenderContext(Device& device, uint32_t frames_inflight);
    ~RenderContext();

    [[nodiscard]] uint32_t current_frame_index() const { return _current_frame; }
    void                   next_frame() { _current_frame = (_current_frame + 1) % _frames_inflight; }

    [[nodiscard]] const vk::Fence&     current_fence_render() const { return _fence_render[_current_frame]; }
    [[nodiscard]] const vk::Fence&     current_fence_transfer() const { return _fence_transfer[_current_frame]; }
    [[nodiscard]] const vk::Semaphore& current_semaphore_acquire() const { return _semaphore_acquire[_current_frame]; }
    [[nodiscard]] const vk::Semaphore& current_semaphore_render() const { return _semaphore_render[_current_frame]; }
    [[nodiscard]] const vk::Semaphore& current_semaphore_transfer() const
    {
        return _semaphore_transfer[_current_frame];
    }
    [[nodiscard]] const vk::CommandBuffer& current_command_buffer_graphics() const
    {
        return _command_buffers_graphics[_current_frame];
    }
    [[nodiscard]] const vk::CommandBuffer& current_command_buffer_present() const
    {
        return _command_buffers_present[_current_frame];
    }


private:
    const Device&  _device;
    const uint32_t _frames_inflight;
    uint32_t       _current_frame = 0;    // 当前 frame 的 index

    std::vector<vk::CommandBuffer> _command_buffers_graphics = {};
    std::vector<vk::CommandBuffer> _command_buffers_present  = {};
    std::vector<vk::Fence>         _fence_render             = {};    // 初始状态为: signaled
    std::vector<vk::Fence>         _fence_transfer           = {};    // 用于 queue family ownership transfe
    std::vector<vk::Semaphore>     _semaphore_acquire        = {};    // swapchain 获得 image 是否可用
    std::vector<vk::Semaphore>     _semaphore_render         = {};    // graphics render 是否完成
    std::vector<vk::Semaphore>     _semaphore_transfer       = {};    // 用于 queue family ownership transfer
};

}    // namespace Hiss

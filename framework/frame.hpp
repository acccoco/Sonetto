#pragma once
#include "device.hpp"


namespace Hiss
{
class Frame
{
public:
    explicit Frame(Device& device);
    ~Frame();

    [[nodiscard]] vk::CommandBuffer command_buffer_graphics() const { return _command_buffer_graphics; }
    [[nodiscard]] vk::CommandBuffer command_buffer_present() const { return _command_buffer_present; }
    [[nodiscard]] vk::CommandBuffer command_buffer_compute() const { return _command_buffer_compute; }


    [[nodiscard]] vk::Semaphore semaphore_swapchain_acquire() const { return _semaphore_swapchain_acquire; }
    [[nodiscard]] vk::Semaphore semaphore_render_complete() const { return _semaphore_render_complete; }
    [[nodiscard]] vk::Semaphore semaphore_transfer_done() const { return _semaphore_transfer_done; }

    void                    fence_push(vk::Fence fence) { _fences.push_back(fence); }
    std::vector<vk::Fence>& fence_get() { return _fences; }
    std::vector<vk::Fence>  fence_pop_all();

private:
    const Device& _device;

    vk::CommandBuffer _command_buffer_graphics = VK_NULL_HANDLE;
    vk::CommandBuffer _command_buffer_present  = VK_NULL_HANDLE;
    vk::CommandBuffer _command_buffer_compute  = VK_NULL_HANDLE;

    std::vector<vk::Fence> _fences = {};    // 当前周期的 frame signal，下一周期的 frame wait

    vk::Semaphore _semaphore_swapchain_acquire = VK_NULL_HANDLE;
    vk::Semaphore _semaphore_render_complete   = VK_NULL_HANDLE;
    vk::Semaphore _semaphore_transfer_done     = VK_NULL_HANDLE;
};
}    // namespace Hiss
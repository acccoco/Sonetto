#include "render_context.hpp"


/**
 * @param frames_inflight swapchain 中 image 的数量
 */
Hiss::RenderContext::RenderContext(Device &device, uint32_t frames_inflight)
    : _device(device),
      _frames_inflight(frames_inflight)
{
    _fence_render.reserve(frames_inflight);
    for (uint32_t i = 0; i < frames_inflight; ++i)
    {
        _fence_render.push_back(_device.vkdevice().createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));
        _fence_transfer.push_back(_device.vkdevice().createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));

        _semaphore_acquire.push_back(_device.vkdevice().createSemaphore({}));
        _semaphore_render.push_back(_device.vkdevice().createSemaphore({}));
        _semaphore_transfer.push_back(_device.vkdevice().createSemaphore({}));
    }


    _command_buffers_graphics = _device.graphics_command_pool().command_buffer_create(frames_inflight);
    _command_buffers_present  = _device.present_command_pool().command_buffer_create(frames_inflight);
}


Hiss::RenderContext::~RenderContext()
{
    for (uint32_t i = 0; i < _frames_inflight; ++i)
    {
        _device.vkdevice().destroy(_fence_render[i]);
        _device.vkdevice().destroy(_fence_transfer[i]);
        _device.vkdevice().destroy(_semaphore_acquire[i]);
        _device.vkdevice().destroy(_semaphore_render[i]);
        _device.vkdevice().destroy(_semaphore_transfer[i]);
    }

    _device.vkdevice().free(_device.graphics_command_pool().pool_get(), _command_buffers_graphics);
    _device.vkdevice().free(_device.present_command_pool().pool_get(), _command_buffers_present);
}
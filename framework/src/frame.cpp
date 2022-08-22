#include "frame.hpp"


Hiss::Frame::Frame(Hiss::Device& device)
    : _device(device)
{
    _command_buffer_graphics = _device.command_pool_graphics().command_buffer_create(1).front();
    _command_buffer_compute  = _device.command_pool_compute().command_buffer_create(1).front();
    _command_buffer_present  = _device.command_pool_present().command_buffer_create(1).front();

    _semaphore_render_complete   = _device.vkdevice().createSemaphore(vk::SemaphoreCreateInfo{});
    _semaphore_swapchain_acquire = _device.vkdevice().createSemaphore(vk::SemaphoreCreateInfo{});
    _semaphore_transfer_done     = _device.vkdevice().createSemaphore(vk::SemaphoreCreateInfo{});
}


Hiss::Frame::~Frame()
{
    _device.vkdevice().destroy(_semaphore_transfer_done);
    _device.vkdevice().destroy(_semaphore_swapchain_acquire);
    _device.vkdevice().destroy(_semaphore_render_complete);

    _device.fence_pool().revert(_fences);
    _fences.clear();

    // command buffer 可以随着 pool 一起销毁
}


std::vector<vk::Fence> Hiss::Frame::fence_pop_all()
{
    std::vector<vk::Fence> fences = {};
    std::swap(fences, _fences);
    return fences;
}
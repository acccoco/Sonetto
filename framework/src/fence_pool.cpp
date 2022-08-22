#include "fence_pool.hpp"
#include "device.hpp"


Hiss::FencePool::~FencePool()
{
    for (auto fence: _pool)
        _device.vkdevice().destroy(fence);
}


vk::Fence Hiss::FencePool::get(bool signaled)
{
    if (_pool.empty())
    {
        _pool.push_back(_device.vkdevice().createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));
    }

    vk::Fence fence = _pool.back();
    _pool.pop_back();

    if (!signaled)
    {
        _device.vkdevice().resetFences({fence});
    }
    return fence;
}


void Hiss::FencePool::revert(vk::Fence fence)
{
    assert(vk::Result::eSuccess == _device.vkdevice().getFenceStatus(fence));
    // _device.vkdevice().resetFences({fence});
    _pool.push_back(fence);
}


void Hiss::FencePool::revert(const std::vector<vk::Fence>& fences)
{
    for (auto& fence: fences)
    {
        revert(fence);
    }
}
#include "vk/fence_pool.hpp"
#include "vk/device.hpp"


Hiss::FencePool::~FencePool()
{
    for (auto fence: _all_fences)
        _device.vkdevice().destroy(fence);
}


vk::Fence Hiss::FencePool::acquire(bool signaled)
{
    if (_available_fences.empty())
    {
        vk::Fence fence = _device.vkdevice().createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
        _available_fences.push_back(fence);
        _all_fences.push_back(fence);
    }

    vk::Fence fence = _available_fences.back();
    _available_fences.pop_back();

    // 从 pool 中出来的 fence 都是 signaled 状态，按照要求将 fence 置为相应状态
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
    _available_fences.push_back(fence);
}


void Hiss::FencePool::revert(const std::vector<vk::Fence>& fences)
{
    for (auto& fence: fences)
    {
        revert(fence);
    }
}
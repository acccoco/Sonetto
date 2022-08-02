#include "fence_pool.hpp"
#include "device.hpp"


Hiss::FencePool::~FencePool()
{
    for (auto fence: _pool)
        _device.vkdevice().destroy(fence);
}


/**
 * 初始状态: unsignaled
 */
vk::Fence Hiss::FencePool::get()
{
    if (_pool.empty())
    {
        _pool.push_back(_device.vkdevice().createFence({}));
    }
    vk::Fence fence = _pool.back();
    _pool.pop_back();
    return fence;
}


/**
 * @param fence 确保 fence 状态: signaled
 */
void Hiss::FencePool::revert(vk::Fence fence)
{
    assert(vk::Result::eSuccess == _device.vkdevice().getFenceStatus(fence));
    _device.vkdevice().resetFences({fence});
    _pool.push_back(fence);
}
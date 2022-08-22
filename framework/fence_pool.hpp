#pragma once
#include "vk_common.hpp"
#include <vector>


namespace Hiss
{
class Device;


class FencePool
{
public:
    explicit FencePool(Device& deivce)
        : _device(deivce)
    {}
    ~FencePool();

    /**
     * 从 fence pool 中获取 fence，可以决定 fence 的初始状态
     */
    vk::Fence get(bool signaled = false);

    /**
     * 将 fence 归还给 fence pool，确保 fence 状态为 signaled
     */
    void revert(vk::Fence fence);
    void revert(const std::vector<vk::Fence>& fences);

private:
    const Device&          _device;
    std::vector<vk::Fence> _pool = {};    // fence 的状态：signaled
};

}    // namespace Hiss
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
     * 从 fence pool 中获取 fence
     */
    vk::Fence get();

    /**
     * 将 fence 归还给 fence pool
     */
    void revert(vk::Fence fence);

private:
    const Device&          _device;
    std::vector<vk::Fence> _pool = {};
};

}    // namespace Hiss
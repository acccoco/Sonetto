#pragma once
#include "core/vk_common.hpp"
#include <vector>


namespace Hiss
{
class Device;


/**
 * 资源管理：
 * - 用完记得归还，方便复用
 * - 外部不应该 destroy fence, 在销毁时会自动 destroy 所有的 fence
 */
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
    vk::Fence acquire(bool signaled = false);

    /**
     * 将 fence 归还给 fence pool，确保 fence 状态为 signaled
     */
    void revert(vk::Fence fence);
    void revert(const std::vector<vk::Fence>& fences);

private:
    Device&                _device;
    std::vector<vk::Fence> _available_fences = {};    // fence 的状态：signaled
    std::vector<vk::Fence> _all_fences       = {};

    const uint32_t MAX_NUMBER = 64;
};

}    // namespace Hiss
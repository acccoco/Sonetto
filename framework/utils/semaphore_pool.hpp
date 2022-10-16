#pragma once
#include "core/vk_common.hpp"
#include "core/device.hpp"


namespace Hiss
{

/**
 * 资源管理：
 * \n - 用完记得归还，方便复用
 * \n - 外部不应该 destroy semaphore，pool 在销毁时会自动 destroy 所有的 semaphore
 */
class SemaphorePool
{
public:
    explicit SemaphorePool(Device& device)
        : _device(device)
    {}

    ~SemaphorePool()
    {
        for (auto semaphore: this->_all_semaphores)
            _device.vkdevice().destroy(semaphore);
    }

    /**
     * 向 pool 申请一个 semaphore，状态是 unsignaled
     */
    vk::Semaphore acquire()
    {
        if (_available_semaphores.empty())
        {
            auto semaphore = _device.vkdevice().createSemaphore(vk::SemaphoreCreateInfo());
            _available_semaphores.push_back(semaphore);
            _all_semaphores.push_back(semaphore);

            if (_all_semaphores.size() > MAX_NUMBER)
                spdlog::warn("semaphore pool has too many semaphore: {}", _all_semaphores.size());
        }

        auto semaphore = this->_available_semaphores.back();
        this->_available_semaphores.pop_back();
        return semaphore;
    }

    void revert(vk::Semaphore semaphore) { _available_semaphores.push_back(semaphore); }


private:
    Device& _device;


    // 当前 pool 中可用的 semaphore
    std::vector<vk::Semaphore> _available_semaphores = {};

    // 从当前 pool 创建的所有 semaphore
    std::vector<vk::Semaphore> _all_semaphores = {};

    const uint32_t MAX_NUMBER = 64;
};
}    // namespace Hiss
#pragma once
#include "vk/vk_common.hpp"
#include "vk/device.hpp"


namespace Hiss
{

/**
 * 资源管理：
 * - 用完记得归还，方便复用
 * - 外部不应该 destroy semaphore，pool 在销毁时会自动 destroy 所有的 semaphore
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

    std::vector<vk::Semaphore> _available_semaphores = {};
    std::vector<vk::Semaphore> _all_semaphores       = {};

    const uint32_t MAX_NUMBER = 64;
};
}    // namespace Hiss
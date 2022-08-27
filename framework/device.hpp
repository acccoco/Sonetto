#pragma once
#include "vk_common.hpp"
#include "window.hpp"
#include "physical_device.hpp"
#include "command.hpp"
#include "fence_pool.hpp"


namespace Hiss
{

enum class QueueFlag
{
    Graphics,
    Present,
    Compute,
    Transfer,
};


struct Queue
{
    vk::Queue queue        = VK_NULL_HANDLE;
    uint32_t  family_index = {};
    QueueFlag flag         = {};


    static bool is_same_queue_family(const Queue& q1, const Queue& q2) { return q1.family_index == q2.family_index; }
};


class Device
{
public:
    Device(GPU& physical_device_, spdlog::logger& logger);
    ~Device();

    [[nodiscard]] vk::Device      vkdevice() const { return _device; }
    [[nodiscard]] const GPU&      gpu_get() const { return _gpu; }
    [[nodiscard]] spdlog::logger& logger() const { return _logger; }
    [[nodiscard]] const Queue&    queue_graphics() const { return _queue_graphics; }
    [[nodiscard]] const Queue&    queue_present() const { return _queue_present; }
    [[nodiscard]] const Queue&    queue_compute() const { return _queue_compute; }
    [[nodiscard]] CommandPool&    command_pool_graphics() const { return *_command_pool_graphics; }
    [[nodiscard]] CommandPool&    command_pool_present() const { return *_command_pool_present; }
    [[nodiscard]] CommandPool&    command_pool_compute() const { return *_command_pool_compute; }
    [[nodiscard]] FencePool&      fence_pool() const { return *_fence_pool; }

    [[nodiscard]] vk::DeviceMemory memory_allocate(const vk::MemoryRequirements&  mem_require,
                                                   const vk::MemoryPropertyFlags& mem_prop) const;
    vk::Semaphore                  semaphore_create(bool signal = false);

    template<class handle_t>
    void debug_obj_name_set(vk::ObjectType type, handle_t handle, std::string name) const;


private:
    void logical_device_create();
    void command_pool_create();

    // members =======================================================

private:
    GPU&            _gpu;
    spdlog::logger& _logger;
    vk::Device      _device = VK_NULL_HANDLE;


    Queue        _queue_graphics        = {};
    Queue        _queue_present         = {};
    Queue        _queue_compute         = {};
    CommandPool* _command_pool_graphics = nullptr;
    CommandPool* _command_pool_compute  = nullptr;
    CommandPool* _command_pool_present  = nullptr;
    FencePool*   _fence_pool            = nullptr;
};
}    // namespace Hiss


template<class handle_t>
void Hiss::Device::debug_obj_name_set(vk::ObjectType type, handle_t handle, std::string name) const
{
    _device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{
            .objectType   = type,
            .objectHandle = (uint64_t) handle,
            .pObjectName  = name.c_str(),
    });
}
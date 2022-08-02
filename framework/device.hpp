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
    [[nodiscard]] const Queue&    graphics_queue() const { return _graphics_queue; }
    [[nodiscard]] const Queue&    present_queue() const { return _present_queue; }
    [[nodiscard]] const Queue&    compute_queue() const { return _compute_queue; }
    [[nodiscard]] CommandPool&    graphics_command_pool() const { return *_graphics_command_pool; }
    [[nodiscard]] CommandPool&    present_command_pool() const { return *_present_command_pool; }
    [[nodiscard]] FencePool&      fence_pool() const { return *_fence_pool; }

    [[nodiscard]] vk::DeviceMemory memory_allocate(const vk::MemoryRequirements&  mem_require,
                                                   const vk::MemoryPropertyFlags& mem_prop) const;


private:
    void logical_device_create();
    void command_pool_create();

    // members =======================================================

private:
    GPU&            _gpu;
    spdlog::logger& _logger;
    vk::Device      _device = VK_NULL_HANDLE;


    Queue        _graphics_queue        = {};
    Queue        _present_queue         = {};
    Queue        _compute_queue         = {};
    CommandPool* _graphics_command_pool = nullptr;
    CommandPool* _compute_command_pool  = nullptr;
    CommandPool* _present_command_pool  = nullptr;
    FencePool*   _fence_pool            = nullptr;
};
}    // namespace Hiss
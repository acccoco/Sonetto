#pragma once
#include "vk/vk_common.hpp"
#include "vk/queue.hpp"
#include "window.hpp"
#include "gpu.hpp"
#include "vk/command.hpp"
#include "fence_pool.hpp"


namespace Hiss
{
class Device
{
public:
    explicit Device(GPU& physical_device_);
    ~Device();


    [[nodiscard]] vk::DeviceMemory allocate_memory(const vk::MemoryRequirements&  mem_require,
                                                   const vk::MemoryPropertyFlags& mem_prop) const;

    vk::Semaphore create_semaphore(bool signal = false);

    template<class handle_t>
    void set_debug_name(vk::ObjectType type, handle_t handle, const std::string& name) const;


private:
    void create_logical_device();
    void create_command_pool();


    // members =======================================================
public:
    Prop<Queue, Device>      queue{};
    Prop<vk::Device, Device> vkdevice{VK_NULL_HANDLE};

    [[nodiscard]] vk::Queue    vkqueue() const { return this->queue().queue; }
    [[nodiscard]] const GPU&   gpu() const { return _gpu; }
    [[nodiscard]] CommandPool& command_pool() const { return *_command_pool; }
    [[nodiscard]] FencePool&   fence_pool() const { return *_fence_pool; }


private:
    GPU& _gpu;

    CommandPool* _command_pool = nullptr;
    FencePool*   _fence_pool   = nullptr;
};
}    // namespace Hiss


template<class handle_t>
void Hiss::Device::set_debug_name(vk::ObjectType type, handle_t handle, const std::string& name) const
{
    vkdevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{
            .objectType   = type,
            .objectHandle = (uint64_t) handle,
            .pObjectName  = name.c_str(),
    });
}
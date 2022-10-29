#pragma once
#include "vk_common.hpp"
#include "core/queue.hpp"
#include "core/window.hpp"
#include "gpu.hpp"
#include "command.hpp"
#include "utils/fence_pool.hpp"


namespace Hiss
{
class Device
{
public:
    explicit Device(GPU& physical_device_);
    ~Device();


#pragma region 工具方法
    vk::DeviceMemory allocate_memory(const vk::MemoryRequirements&  mem_require,
                                     const vk::MemoryPropertyFlags& mem_prop) const;

    vk::Semaphore create_semaphore(const std::string& debug_name = "", bool signal = false);

    template<class handle_t>
    void set_debug_name(vk::ObjectType type, handle_t handle, const std::string& name) const;


    vk::CommandBuffer create_commnad_buffer(const std::string& debug_name)
    {
        return _command_pool->command_buffer_create(debug_name);
    }


#pragma endregion


#pragma region 初始化数据的方法
private:
    void create_logical_device();
    void create_command_pool();
#pragma endregion


#pragma region 公开的属性
public:
    Prop<vk::Device, Device> vkdevice{VK_NULL_HANDLE};

    Queue&       queue() const { return *this->_queue; }
    vk::Queue    vkqueue() const { return this->queue().vkqueue(); }
    GPU&         gpu() const { return _gpu; }
    CommandPool& command_pool() const { return *_command_pool; }
    FencePool&   fence_pool() const { return *_fence_pool; }

#pragma endregion


#pragma region 私有成员字段
private:
    GPU& _gpu;

    Queue*       _queue        = nullptr;
    CommandPool* _command_pool = nullptr;
    FencePool*   _fence_pool   = nullptr;
#pragma endregion
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
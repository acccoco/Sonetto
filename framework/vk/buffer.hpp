#pragma once
#include "vk/device.hpp"


namespace Hiss
{
class Buffer
{
public:
    Buffer(Hiss::Device& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
           vk::MemoryPropertyFlags memory_properties);
    virtual ~Buffer();

    /**
     * 通过 stage buffer 这个中间过程来创建 buffer
     */
    static Hiss::Buffer* buffer_create_stage(Hiss::Device& device, Hiss::CommandPool& command_pool, vk::DeviceSize size,
                                             vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memory_properties,
                                             void* data);

    [[nodiscard]] vk::Buffer     vkbuffer() const { return _buffer; }
    [[nodiscard]] vk::DeviceSize buffer_size() const { return _buffer_size; }

    void* memory_map();
    void  memory_unmap();
    void  memory_copy_in(const void* data, size_t n);

protected:
    const Hiss::Device& _device;
    vk::Buffer          _buffer      = VK_NULL_HANDLE;
    vk::DeviceMemory    _memory      = VK_NULL_HANDLE;
    vk::DeviceSize      _buffer_size = 0;
    bool                _map         = false;
    void*               _data        = nullptr;
};

}    // namespace Hiss

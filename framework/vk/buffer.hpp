#pragma once
#include "vk/device.hpp"


namespace Hiss
{

// device local 的 buffer
class DeviceBuffer
{
public:
    DeviceBuffer(VmaAllocator allocator, vk::DeviceSize size, VkBufferUsageFlags buffer_usage)
        : size(size),
          _allocator(allocator)
    {
        VkBufferCreateInfo indices_buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = size,
                .usage = buffer_usage,
        };
        VmaAllocationCreateInfo indices_alloc_create_info = {
                .flags    = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .usage    = VMA_MEMORY_USAGE_AUTO,
                .priority = 1.f,
        };
        vmaCreateBuffer(allocator, &indices_buffer_info, &indices_alloc_create_info, &buffer._value, &_allocation,
                        nullptr);
    }

    ~DeviceBuffer() { vmaDestroyBuffer(_allocator, buffer._value, _allocation); }


public:
    Prop<VkBuffer, DeviceBuffer>       buffer{VK_NULL_HANDLE};
    Prop<vk::DeviceSize, DeviceBuffer> size{};


private:
    VmaAllocator  _allocator{};
    VmaAllocation _allocation{};
};


class Buffer
{
public:
    Buffer(const Hiss::Device& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
           vk::MemoryPropertyFlags memory_properties);
    virtual ~Buffer();

    /**
     * 通过 stage buffer 这个中间过程来创建 buffer
     */
    static Hiss::Buffer* buffer_create_stage(Hiss::Device& device, Hiss::CommandPool& command_pool, vk::DeviceSize size,
                                             vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memory_properties,
                                             void* data);

    [[nodiscard]] vk::Buffer vkbuffer() const { return _buffer; }

    void* memory_map();
    void  memory_unmap();
    void  memory_copy_in(const void* data, size_t n);


public:
    Prop<vk::DeviceSize, Buffer> buffer_size{0};


protected:
    const Hiss::Device& _device;
    vk::Buffer          _buffer = VK_NULL_HANDLE;
    vk::DeviceMemory    _memory = VK_NULL_HANDLE;
    bool                _map    = false;
    void*               _data   = nullptr;
};


class StageBuffer
{
public:
    StageBuffer(VmaAllocator allocator, vk::DeviceSize size)
        : _allocator(allocator),
          _size(size)
    {
        assert(size > 0);


        // 创建 stage buffer
        VkBufferCreateInfo buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        VmaAllocationCreateInfo alloc_create_info = {
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO,
        };
        vmaCreateBuffer(allocator, &buffer_info, &alloc_create_info, &buffer._value, &_allocation, &_alloc_info);
    }

    ~StageBuffer() { vmaDestroyBuffer(_allocator, buffer._value, _allocation); }


    // 将内存拷贝到 stage buffer 中
    void mem_copy(const void* data, vk::DeviceSize size) const
    {
        assert(_size >= size);

        std::memcpy(_alloc_info.pMappedData, data, size);
    }


public:
    Prop<VkBuffer, StageBuffer> buffer{};


private:
    VmaAllocator      _allocator;
    VmaAllocation     _allocation{};    // 对应 memory
    VmaAllocationInfo _alloc_info{};    // 分配信息，例如内存映射的地址
    vk::DeviceSize    _size;
};

}    // namespace Hiss

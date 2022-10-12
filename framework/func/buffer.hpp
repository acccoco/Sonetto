#pragma once
#include "core/device.hpp"


namespace Hiss
{

class Buffer2
{
public:
    Buffer2(VmaAllocator allocator, vk::DeviceSize size, vk::BufferUsageFlags buffer_usage,
            VmaAllocationCreateFlags memory_flags)
        : size(size),
          _allocator(allocator)
    {
        assert(size > 0);

        VkBufferCreateInfo indices_buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = size,
                .usage = (VkBufferUsageFlags) buffer_usage,
        };
        VmaAllocationCreateInfo indices_alloc_create_info = {
                .flags    = memory_flags,
                .usage    = VMA_MEMORY_USAGE_AUTO,
                .priority = 1.f,
        };
        vmaCreateBuffer(allocator, &indices_buffer_info, &indices_alloc_create_info, &vkbuffer._value, &_allocation,
                        &_alloc_info);
    }

    ~Buffer2() { vmaDestroyBuffer(_allocator, vkbuffer._value, _allocation); }


    // 在 command buffer 中插入关于当前 buffer 的内存屏障
    void memory_barrier(vk::CommandBuffer& command_buffer, const StageAccess& src, const StageAccess& dst)
    {
        command_buffer.pipelineBarrier(src.stage, dst.stage, {}, {},
                                       vk::BufferMemoryBarrier{
                                               .srcAccessMask = src.access,
                                               .dstAccessMask = dst.access,
                                               .buffer        = vkbuffer._value,
                                               .offset        = 0,
                                               .size          = size._value,
                                       },
                                       {});
    }


#pragma region public properties
public:
    Prop<VkBuffer, Buffer2>       vkbuffer{};
    Prop<vk::DeviceSize, Buffer2> size;

#pragma endregion


#pragma region private members
protected:
    VmaAllocator      _allocator;
    VmaAllocation     _allocation{};    // 对应 memory
    VmaAllocationInfo _alloc_info{};    // 分配信息，例如内存映射的地址

#pragma endregion
};



// 用于从 CPU 到 GPU 传递数据的 buffer
class StageBuffer : public Buffer2
{
public:
    // memory flags: HOST_VISIBLE, 自动 mapped
    StageBuffer(VmaAllocator allocator, vk::DeviceSize size)
        : Buffer2(allocator, size, vk::BufferUsageFlagBits::eTransferSrc,
                  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
    {}


    // 将内存拷贝到 stage buffer 中
    void mem_copy(const void* src, vk::DeviceSize size) const
    {
        assert(this->size() >= size);

        std::memcpy(_alloc_info.pMappedData, src, size);
    }
};


class UniformBuffer : public Buffer2
{
public:
    // memory flags: HOST_VISIBLE, DEVICE_LOCAL, 自动 mapped
    UniformBuffer(VmaAllocator allocator, vk::DeviceSize size)
        : Buffer2(allocator, size, vk::BufferUsageFlagBits::eUniformBuffer,
                  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                          | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                          | VMA_ALLOCATION_CREATE_MAPPED_BIT)
    {}


    // 向 uniform buffer 中拷贝数据
    void memory_copy(const void* src, vk::DeviceSize size)
    {
        assert(this->size() >= size);

        std::memcpy(_alloc_info.pMappedData, src, size);
    }
};


}    // namespace Hiss

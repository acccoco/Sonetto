#pragma once
#include "core/device.hpp"


namespace Hiss
{

class Buffer
{
public:
    Buffer(const Hiss::Device& device, VmaAllocator allocator, vk::DeviceSize size, vk::BufferUsageFlags buffer_usage,
           VmaAllocationCreateFlags memory_flags, const std::string& name)
        : size(size),
          name(name),
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

        device.set_debug_name(vk::ObjectType::eBuffer, vkbuffer._value, this->name);
    }

    ~Buffer() { vmaDestroyBuffer(_allocator, vkbuffer._value, _allocation); }


    /**
     * 在 command buffer 中插入关于当前 buffer 的内存屏障
     */
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


    /**
     * 将内存拷贝到 stage buffer 中
     * @details 需要由 application 确保 buffer 是可以 map 的
     */
    void mem_copy(const void* src, vk::DeviceSize src_size) const
    {
        assert(this->size() >= src_size);

        std::memcpy(_alloc_info.pMappedData, src, src_size);
    }


public:
    Prop<VkBuffer, Buffer>       vkbuffer{};
    Prop<vk::DeviceSize, Buffer> size;
    std::string                  name;


protected:
    VmaAllocator      _allocator;
    VmaAllocation     _allocation{};    // 对应 memory
    VmaAllocationInfo _alloc_info{};    // 分配信息，例如内存映射的地址
};


// 用于从 CPU 到 GPU 传递数据的 buffer
class StageBuffer : public Buffer
{
public:
    // memory flags: HOST_VISIBLE, 自动 mapped
    StageBuffer(const Hiss::Device& device, VmaAllocator allocator, vk::DeviceSize size, const std::string& name)
        : Buffer(device, allocator, size, vk::BufferUsageFlagBits::eTransferSrc,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, name)
    {}
};


class UniformBuffer : public Buffer
{
public:
    /**
     * memory flags: HOST_VISIBLE, DEVICE_LOCAL, 自动 mapped
     * @param data  可以使用这些内存进行初始化
     */
    UniformBuffer(const Hiss::Device& device, VmaAllocator allocator, vk::DeviceSize size, const std::string& name = "",
                  const void* data = nullptr)
        : Buffer(device, allocator, size, vk::BufferUsageFlagBits::eUniformBuffer,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                         | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                         | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                 name)
    {
        if (data)
            mem_copy(data, this->size());
    }
};


}    // namespace Hiss

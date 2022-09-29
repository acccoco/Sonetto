#include "vk/buffer.hpp"
#include <cstring>


/**
 * buffer 的 sharing mode 是 Exclusive
 */
Hiss::Buffer::Buffer(const Hiss::Device& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
                     vk::MemoryPropertyFlags memory_properties)
    : buffer_size(size),
      _device(device)
{
    _buffer = device.vkdevice().createBuffer(vk::BufferCreateInfo{
            .size        = size,
            .usage       = usage,
            .sharingMode = vk::SharingMode::eExclusive,
    });

    vk::MemoryRequirements memory_require = _device.vkdevice().getBufferMemoryRequirements(_buffer);
    _memory                               = _device.allocate_memory(memory_require, memory_properties);
    _device.vkdevice().bindBufferMemory(_buffer, _memory, 0);
}


Hiss::Buffer::~Buffer()
{
    this->memory_unmap();
    _device.vkdevice().destroy(_buffer);
    _device.vkdevice().free(_memory);
}


Hiss::Buffer* Hiss::Buffer::buffer_create_stage(Hiss::Device& device, Hiss::CommandPool& command_pool,
                                                vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                vk::MemoryPropertyFlags memory_properties, void* data)
{
    auto stage_buffer =
            Hiss::Buffer(device, size, vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    stage_buffer.memory_copy_in(data, size);

    auto buffer = new Hiss::Buffer(device, size,
                                   vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
                                           | vk::BufferUsageFlagBits::eStorageBuffer,
                                   vk::MemoryPropertyFlagBits::eDeviceLocal);

    Hiss::OneTimeCommand command_buffer{device, command_pool};
    command_buffer().copyBuffer(stage_buffer.vkbuffer(), buffer->vkbuffer(), {vk::BufferCopy{.size = size}});
    command_buffer.exec();

    return buffer;
}


void* Hiss::Buffer::memory_map()
{
    if (!_map)
    {
        _data = _device.vkdevice().mapMemory(_memory, 0, buffer_size(), {});
        _map  = true;
    }
    return _data;
}


void Hiss::Buffer::memory_unmap()
{
    if (!_map)
        return;
    _device.vkdevice().unmapMemory(_memory);
    _map = false;
}


void Hiss::Buffer::memory_copy_in(const void* data, size_t n)
{
    std::memcpy(this->memory_map(), data, n);
}

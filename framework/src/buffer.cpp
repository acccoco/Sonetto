#include "buffer.hpp"
#include <cstring>


/**
 * buffer 的 sharing mode 是 Exclusive
 */
Hiss::Buffer::Buffer(Hiss::Device& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
                     vk::MemoryPropertyFlags memory_properties)
    : _device(device),
      _buffer_size(size)
{
    _buffer = device.vkdevice().createBuffer(vk::BufferCreateInfo{
            .size        = size,
            .usage       = usage,
            .sharingMode = vk::SharingMode::eExclusive,
    });

    vk::MemoryRequirements memory_require = _device.vkdevice().getBufferMemoryRequirements(_buffer);
    _memory                               = _device.memory_allocate(memory_require, memory_properties);
    _device.vkdevice().bindBufferMemory(_buffer, _memory, 0);
}


Hiss::Buffer::~Buffer()
{
    this->memory_unmap();
    _device.vkdevice().destroy(_buffer);
    _device.vkdevice().free(_memory);
}


void* Hiss::Buffer::memory_map()
{
    if (!_map)
    {
        _data = _device.vkdevice().mapMemory(_memory, 0, _buffer_size, {});
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

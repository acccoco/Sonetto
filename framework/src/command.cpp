#include "command.hpp"
#include "device.hpp"


Hiss::CommandPool::CommandPool(Device &device, const Queue &queue)
    : _device(device),
      _queue(queue)
{
    _pool = device.vkdevice().createCommandPool(vk::CommandPoolCreateInfo{
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queue.family_index,
    });
}


Hiss::CommandPool::~CommandPool()
{
    _device.vkdevice().destroy(_pool);
}

std::vector<vk::CommandBuffer> Hiss::CommandPool::command_buffer_create(uint32_t count)
{
    return _device.vkdevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo{
            .commandPool        = _pool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = count,
    });
}


Hiss::OneTimeCommand::OneTimeCommand(Hiss::Device &device, Hiss::CommandPool &pool)
    : _device(device),
      _pool(pool)
{
    _command_buffer = pool.command_buffer_create()[0];
    _command_buffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
}


Hiss::OneTimeCommand::~OneTimeCommand()
{
    _device.vkdevice().free(_pool.pool_get(), {_command_buffer});
}


void Hiss::OneTimeCommand::exec()
{
    assert(!_used);

    vk::Fence fence = _device.fence_pool().get(false);

    _command_buffer.end();
    _pool.queue_get().queue.submit({vk::SubmitInfo{
                                           .commandBufferCount = 1,
                                           .pCommandBuffers    = &_command_buffer,
                                   }},
                                   fence);

    (void) _device.vkdevice().waitForFences({fence}, VK_TRUE, UINT64_MAX);
    _device.fence_pool().revert(fence);

    _used = true;
}
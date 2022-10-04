#include "vk/command.hpp"
#include "vk/device.hpp"


Hiss::CommandPool::CommandPool(Device& device, Queue& queue)
    : _device(device),
      _queue(queue)
{
    _pool = device.vkdevice().createCommandPool(vk::CommandPoolCreateInfo{
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queue.queue_family_index(),
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


vk::CommandBuffer Hiss::CommandPool::command_buffer_create(const std::string& name)
{
    auto command_buffer = this->command_buffer_create(1).front();
    _device.set_debug_name(vk::ObjectType::eCommandBuffer, (VkCommandBuffer) command_buffer, name);
    return command_buffer;
}


Hiss::OneTimeCommand::OneTimeCommand(const Hiss::Device& device, Hiss::CommandPool& pool)
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
    _device.vkdevice().free(_pool.vkpool(), {_command_buffer});
}


void Hiss::OneTimeCommand::exec()
{
    assert(!_used);

    vk::Fence fence = _device.fence_pool().acquire(false);

    _command_buffer.end();
    _pool.queue().vkqueue().submit({vk::SubmitInfo{
                                           .commandBufferCount = 1,
                                           .pCommandBuffers    = &_command_buffer,
                                   }},
                                   fence);

    (void) _device.vkdevice().waitForFences({fence}, VK_TRUE, UINT64_MAX);
    _device.fence_pool().revert(fence);

    _used = true;
}
#pragma once
#include "vk_common.hpp"


namespace Hiss
{

class Device;
struct Queue;


/**
 * command pool 并不和具体的 queue 绑定，而是和 queue family 绑定
 * 但是这里折衷一下
 */
class CommandPool
{
public:
    CommandPool(Device& device, const Queue& queue);
    ~CommandPool();

    [[nodiscard]] vk::CommandPool pool_get() const { return _pool; }
    [[nodiscard]] const Queue&    queue_get() const { return _queue; }

    std::vector<vk::CommandBuffer> command_buffer_create(uint32_t count = 1);

    /**
     * 创建一个 command buffer，并注册用于 debug 的 object name
     */
    vk::CommandBuffer command_buffer_create(std::string name);

private:
    const Device&   _device;
    const Queue&    _queue;
    vk::CommandPool _pool = VK_NULL_HANDLE;
};


/**
 * 这个类是 RAII 的
 * 使用示例：
 *      OneTimeCommand cmd(device, pool);
 *      cmd().cmdxxx();
 *      cmd.exec();
 */
class OneTimeCommand
{
public:
    OneTimeCommand(Device& device, CommandPool& pool);
    ~OneTimeCommand();

    vk::CommandBuffer& operator()() { return _command_buffer; }
    void               exec();

private:
    const Device&      _device;
    const CommandPool& _pool;
    vk::CommandBuffer  _command_buffer;
    bool               _used{false};
};

}    // namespace Hiss
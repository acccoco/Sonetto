#pragma once
#include <utility>
#include <vector>
#include "vk_config.hpp"
#include "core/vk_common.hpp"
#include "core/device.hpp"
#include "swapchain.hpp"
#include "utils/semaphore_pool.hpp"

namespace Hiss
{


class FrameManager;

/**
 * acquire 和 submit_semaphore 是用于 image 同步的
 * \n app 使用 image 的时序：
 * \n - 检查 acquire_semaphroe
 * \n - 使用 image
 * \n - signal submit_semaphore
 *
 * \n Frame 关键的内容是 image，以及具体 app 的 payload
 * 通过 fences 来保护 payload 中的资源
 */
class Frame
{
public:
    friend FrameManager;


    /**
     * 仅在当前 frame 有效的 command buffer，在下一个周期到来时会被销毁掉
     */
    struct FrameCommandBuffer
    {
        explicit FrameCommandBuffer(Frame& frame, const std::string& name = "",
                                    std::vector<vk::Semaphore> signal_semaphores = {})
            : signal_semaphores(std::move(signal_semaphores)),
              frame(frame)
        {
            command_buffer = frame.acquire_command_buffer(name);
            command_buffer.begin(vk::CommandBufferBeginInfo{});
        }

        ~FrameCommandBuffer()
        {
            command_buffer.end();
            frame._device.queue().submit_commands({}, {command_buffer}, signal_semaphores, frame.insert_fence());
        }

        inline vk::CommandBuffer& operator()() { return command_buffer; }

        vk::CommandBuffer          command_buffer;
        std::vector<vk::Semaphore> signal_semaphores;
        Frame&                     frame;
    };

    Frame(Device& device, uint32_t frame_index, Hiss::Image2D& image)
        : frame_id(frame_index),
          submit_semaphore(device.create_semaphore(fmt::format("frame-{}", frame_index), false)),
          _device(device),
          _image(image)
    {}

    ~Frame()
    {
        _device.vkdevice().destroy(submit_semaphore._value);

        // 归还 fence
        _device.fence_pool().revert(_fences);
    }


    /**
     * 向 frame 加入 fence，用于保护 frame 相关 payload 中的资源
     * @example
     * \n - 申请一个 fence：fence = insert_fence()
     * \n - 使用 fence 来进行同步：use(fence)
     */
    vk::Fence insert_fence()
    {
        auto fence = _device.fence_pool().acquire(false);
        this->_fences.push_back(fence);
        return fence;
    }


    /**
     * 申请一个 command buffer，用于当前 frame。在下一个循环时，该 command buffer 变得不可用
     */
    vk::CommandBuffer acquire_command_buffer(const std::string& name)
    {
        auto command_buffer = _device.create_commnad_buffer(name);
        _command_buffers.push_back(command_buffer);
        return command_buffer;
    }


    /**
     * 等待所有的 fence，并清空 fence 列表。并且将 fence 归还给 pool
     * 销毁当前 frame 分配的临时 command buffer
     */
    void wait_resource()
    {
        if (!_fences.empty())
        {
            (void) _device.vkdevice().waitForFences(_fences, VK_TRUE, UINT64_MAX);
            _device.fence_pool().revert(_fences);
            _fences.clear();
        }

        if (!_command_buffers.empty())
        {
            _device.vkdevice().freeCommandBuffers(_device.command_pool().vkpool(), _command_buffers);
            _command_buffers.clear();
        }
    }


    /**
     * 向默认队列提交命令
     * 不需要等待 semaphore，不需要通知 semaphore，自动插入 fence
     */
    void submit_command(const vk::CommandBuffer& command_buffer)
    {
        _device.queue().submit_commands({}, {command_buffer}, {}, insert_fence());
    }


    // 公共属性=============================================================================================
public:
    // id 和 swapchain image index 是等同的
    Prop<uint32_t, Frame> frame_id;

    Hiss::Image2D& image() const { return _image; }


    /**
     * GPU 绘制完成后，会将这个 semaphore 设为 signaled
     * 之后才会将 image 提交给 swapchain 去绘制
     */
    Prop<vk::Semaphore, Frame> submit_semaphore{VK_NULL_HANDLE};
    // ====================================================================================================

    // 私有成员============================================================================================
private:
    Device& _device;

    Hiss::Image2D& _image;


    // 用于保护和当前 frame 关联的数据
    std::vector<vk::Fence> _fences = {};

    // 当前 frame 申请的所有临时 command buffer
    std::vector<vk::CommandBuffer> _command_buffers = {};
    // ====================================================================================================
};

}    // namespace Hiss
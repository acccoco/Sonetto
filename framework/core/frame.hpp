#pragma once
#include <vector>
#include "vk_config.hpp"
#include "vk_common.hpp"
#include "device.hpp"
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
     * 等待所有的 fence，并清空 fence 列表。
     * 并且将 fence 归还给 pool
     */
    void wait_clear_fence()
    {
        if (_fences.empty())
            return;
        (void) _device.vkdevice().waitForFences(_fences, VK_TRUE, UINT64_MAX);
        _device.fence_pool().revert(_fences);
        _fences.clear();
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
    // ====================================================================================================
};

}    // namespace Hiss
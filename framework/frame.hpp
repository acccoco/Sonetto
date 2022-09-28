#pragma once
#include <vector>
#include "vk/vk_common.hpp"
#include "vk/device.hpp"
#include "vk/swapchain.hpp"
#include "utils/semaphore_pool.hpp"

namespace Hiss
{


class FrameManager;

/**
 * acquire 和 submit_semaphore 是用于 image 同步的
 * app 使用 image 的时序：
 * - 检查 acquire_semaphroe
 * - 使用 image
 * - signal submit_semaphore
 *
 * Frame 关键的内容是 image，以及具体 app 的 payload
 * 通过 fences 来保护 payload 中的资源
 */
class Frame
{
public:
    friend FrameManager;

    Frame(Device& device, FencePool& fence_pool, uint32_t frame_index)
        : frame_id(frame_index),
          _device(device),
          _fence_pool(fence_pool)
    {
        this->submit_semaphore._value = _device.vkdevice().createSemaphore(vk::SemaphoreCreateInfo());
    }

    ~Frame()
    {
        _device.vkdevice().destroy(submit_semaphore._value);

        _fences.clear();
    }

    // 向 frame 加入 fence，用于保护 frame 相关 payload 中的资源
    vk::Fence insert_fence()
    {
        auto fence = this->_fence_pool.acquire(false);
        this->_fences.push_back(fence);
        return fence;
    }

public:
    Prop<vk::Image, FrameManager>     image{VK_NULL_HANDLE};
    Prop<vk::ImageView, FrameManager> image_view{VK_NULL_HANDLE};
    Prop<uint32_t, Frame>             frame_id;

    // 后续想要使用 frame 内的 image，需要确保这个 semaphore 是 signaled
    Prop<vk::Semaphore, Frame> submit_semaphore{VK_NULL_HANDLE};

    // 该变量不是在 frame 中创建的
    // 使用 image 之前需要确保该 semaphore 是 signaled
    Prop<vk::Semaphore, FrameManager> acquire_semaphore{VK_NULL_HANDLE};


private:
    Device&    _device;
    FencePool& _fence_pool;

    std::vector<vk::Fence> _fences = {};
};


class FrameManager
{
public:
    FrameManager(Device& device, Swapchain& swapchain)
        : _device(device),
          _swapchain(swapchain)
    {
        this->_semaphore_pool = new SemaphorePool(_device);
        this->_fence_pool     = new FencePool(_device);


        /**
         * 创建 frames
         * 为 frame 放入一个 acquire semaphore，确保 acquire semaphore 一定是非空的
         */
        this->frames._value.reserve(_swapchain.get_image_number());
        for (int id = 0; id < _swapchain.get_image_number(); ++id)
        {
            auto frame                      = new Frame(_device, *_fence_pool, id);
            frame->acquire_semaphore._value = this->_semaphore_pool->acquire();
            frame->image._value             = _swapchain.get_image(id);
            frame->image_view._value        = _swapchain.get_image_view(id);
            this->frames._value.push_back(frame);
        }
    }

    ~FrameManager()
    {
        for (auto frame: this->frames._value)
        {
            // 从 frame 中取回 acquire 的 semaphore
            this->_semaphore_pool->revert(frame->acquire_semaphore.get());
            DELETE(frame);
        }
        DELETE(_semaphore_pool);
        DELETE(_fence_pool);
    }

    // 获取 frame，用于渲染，会等待 fence
    Frame* acquire_frame()
    {
        auto semaphore                    = this->_semaphore_pool->acquire();
        auto [need_recreate, image_index] = this->_swapchain.acquire_image(semaphore);
        if (need_recreate == EnumRecreate::NEED)
            spdlog::warn("[need on_resize] acquire frame, id: {}", image_index);

        assert(image_index < this->frames._value.size());
        auto frame = this->frames._value[image_index];

        // 等待 frame 的所有 fence，保证 frame 内的资源可用
        if (!frame->_fences.empty())
        {
            (void) _device.vkdevice().template waitForFences(frame->_fences, VK_TRUE, UINT64_MAX);
            this->_fence_pool->revert(frame->_fences);
            frame->_fences.clear();
        }

        // 将 acquire semaphore 放入 frame 内
        this->_semaphore_pool->revert(frame->acquire_semaphore.get());
        frame->acquire_semaphore._value = semaphore;

        return frame;
    }

    void submit_frame(Hiss::Frame* frame)
    {
        auto need_recreate = _swapchain.submit_image(frame->frame_id.get(), frame->submit_semaphore.get());
        if (need_recreate == EnumRecreate::NEED)
            spdlog::warn("[need on_resize] submit frame, id: {}", frame->frame_id.get());
    }

    // 销毁原对象，返回新对象
    static FrameManager* resize(FrameManager* old, Device& device, Swapchain& swapchain)
    {
        DELETE(old);
        return new FrameManager(device, swapchain);
    }


public:
    Prop<std::vector<Frame*>, FrameManager> frames;


private:
    Device&    _device;
    Swapchain& _swapchain;


    SemaphorePool* _semaphore_pool = nullptr;
    FencePool*     _fence_pool     = nullptr;
};
}    // namespace Hiss
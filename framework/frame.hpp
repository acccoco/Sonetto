#pragma once
#include <vector>
#include "vk/vk_config.hpp"
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
        this->submit_semaphore._value  = _device.vkdevice().createSemaphore(vk::SemaphoreCreateInfo());
        this->acquire_semaphore._value = device.vkdevice().createSemaphore(vk::SemaphoreCreateInfo());
    }

    ~Frame()
    {
        _device.vkdevice().destroy(submit_semaphore._value);
        _device.vkdevice().destroy(acquire_semaphore._value);

        // 归还 fence
        _fence_pool.revert(_fences);
    }

    // 向 frame 加入 fence，用于保护 frame 相关 payload 中的资源
    // fence 源于类中的 fence_pool
    vk::Fence insert_fence()
    {
        auto fence = this->_fence_pool.acquire(false);
        this->_fences.push_back(fence);
        return fence;
    }


    // 等待所有的 fence，并清空 fence 列表
    void wait_clear_fence()
    {
        if (_fences.empty())
            return;
        (void) _device.vkdevice().waitForFences(_fences, VK_TRUE, UINT64_MAX);
        _fence_pool.revert(_fences);
        _fences.clear();
    }


#pragma region 公开的属性
public:
    Prop<uint32_t, Frame> frame_id;

    [[nodiscard]] Hiss::Image2D& image() const { return *_image; }

    // 后续想要使用 frame 内的 image，需要确保这个 semaphore 是 signaled
    Prop<vk::Semaphore, Frame> submit_semaphore{VK_NULL_HANDLE};

    // 使用 image 之前需要确保该 semaphore 是 signaled
    Prop<vk::Semaphore, Frame> acquire_semaphore{VK_NULL_HANDLE};
#pragma endregion


#pragma region 私有成员字段
private:
    Device&    _device;
    FencePool& _fence_pool;

    Hiss::Image2D* _image = nullptr;

    // frame id 与 swapchain image id 并不相同，frame id 是 frame in flight 中的 id
    uint32_t swapchain_image_index{};

    std::vector<vk::Fence> _fences = {};
#pragma endregion
};


/**
 * frame 和 swapchain 的 image 并不是对应的关系，frame 的数量可以不等于 swapchain image 的数量
 */
class FrameManager
{
public:
    FrameManager(Device& device, Swapchain& swapchain)
        : frames_number(config.frames_number),
          _device(device),
          _swapchain(swapchain)
    {
        this->frames._value.resize(frames_number._value);
        for (int id = 0; id < frames_number._value; ++id)
            this->frames._value[id] = new Frame(_device, device.fence_pool(), id);

        spdlog::info("[frame manager] frame number: {}", frames._value.size());

        // 设定即将渲染的 frame
        _current_frame = frames._value.front();
    }

    ~FrameManager()
    {
        for (auto frame: this->frames._value)
            DELETE(frame);
    }


#pragma region 公开的方法
    // 获取 frame，用于渲染，会等待 fence
    void acquire_frame()
    {
        // 等待 frame 的所有 fence，确保 frame 内的资源可用
        _current_frame->wait_clear_fence();

        auto swapchain_image_index            = this->_swapchain.acquire_image(_current_frame->acquire_semaphore());
        _current_frame->swapchain_image_index = swapchain_image_index;
        _current_frame->_image                = _swapchain.get_image(swapchain_image_index);
    }


    // 提交 current frame 给 swapchain，用于渲染
    void submit_frame()
    {
        assert(_current_frame != nullptr);
        _swapchain.submit_image(_current_frame->swapchain_image_index, _current_frame->submit_semaphore());
        _current_frame = frames._value[(_current_frame->frame_id() + 1) % frames_number._value];
    }

    // 窗口 resize 时调用
    static FrameManager* on_resize(FrameManager* old, Device& device, Swapchain& swapchain)
    {
        DELETE(old);
        return new FrameManager(device, swapchain);
    }
#pragma endregion


#pragma region 公共的属性
public:
    Prop<std::vector<Frame*>, FrameManager> frames;
    Prop<uint32_t, FrameManager>            frames_number{0};

    [[nodiscard]] Frame& current_frame() const { return *_current_frame; }
#pragma endregion


#pragma region 私有成员字段
private:
    Device&    _device;
    Swapchain& _swapchain;

    Frame* _current_frame = nullptr;
#pragma endregion
};
}    // namespace Hiss
#pragma once
#include <vector>
#include "vk_config.hpp"
#include "core/vk_common.hpp"
#include "core/device.hpp"
#include "swapchain.hpp"
#include "utils/semaphore_pool.hpp"
#include "frame.hpp"


namespace Hiss
{

/**
 * FrameManger 中的各个 frame 和 swapchian 中的 image 是一一对应的关系
 * @details 由于从 swapchain 获得的 image 都是 swapchian 拥有的 image（即：渲染已经完成了），
 *  因此，将 frame 和 swapchain 的 image 绑定起来是没有问题的
 * @example
 * \n 使用示例
 * \n - 向 swapchain 获取 image 用于渲染： acquire_frame
 * \n - 使用当前 frame 的资源：use(current_frame)
 * \n - 提交当前 frame 给 swapchain 显示： submit_frame
 */
class FrameManager
{
public:
    FrameManager(Device& device, Swapchain& swapchain)
        : frames_number(swapchain.image_number()),
          _device(device),
          _swapchain(swapchain)
    {
        // 创建 frame
        this->_frames.resize(frames_number._value);
        for (int id = 0; id < frames_number._value; ++id)
            this->_frames[id] = new Frame(_device, id, *_swapchain.get_image(id));
    }

    ~FrameManager()
    {
        for (auto frame: this->_frames)
            delete frame;
    }


    // 获取 frame，用于渲染，会等待 fence
    void acquire_frame()
    {
        /**
         * 向 swapchain 获取 image，并等待 image 可用
         * @details swapchain 可能正在读取 image（presentation engine 的时间周期：读取 image，显示 image），
         *  因此需要 semaphore 或者 fence 来进行同步，使用 fence 并不会损失性能。
         *  留给 CPU 录制 command 和 GPU 渲染的时间有：presentation engine 显示当前 image 的时间；presentation 读取并显示下一个 image 的时间
         */
        auto swapchain_acquire_fence = _device.fence_pool().acquire();
        auto swapchain_image_index   = _swapchain.acquire_image(VK_NULL_HANDLE, swapchain_acquire_fence);
        (void) _device.vkdevice().waitForFences(swapchain_acquire_fence, VK_TRUE, UINT64_MAX);
        _device.fence_pool().revert(swapchain_acquire_fence);


        // 设置 current frame，等待 frame 对应的资源可用
        _current_frame = _frames[swapchain_image_index];
        _current_frame->wait_resource();
    }


    // 提交 current frame 给 swapchain，用于渲染
    void submit_frame()
    {
        assert(_current_frame != nullptr);


        _swapchain.submit_image(_current_frame->frame_id(), _current_frame->submit_semaphore());

        // 提交之后，current frame 就是无效的了
        _current_frame = nullptr;
    }


    // 窗口 resize 时调用
    static FrameManager* on_resize(FrameManager* old, Device& device, Swapchain& swapchain)
    {
        DELETE(old);
        return new FrameManager(device, swapchain);
    }


    // 公开属性 =============================================================================================
public:
    // frame manger 中一共有多少 frame
    Prop<uint32_t, FrameManager> frames_number{0};


    Frame& current_frame() const
    {
        // 在应用刚启动时，或者 submit 之后 && acquire 之前，current frame 是无限的，此时不要调用这个方法
        assert(_current_frame);

        return *_current_frame;
    }


    /**
     * 根据 frame id 获取某一帧
     */
    Frame& frame(uint32_t frame_id) const
    {
        assert(frame_id < frames_number._value);
        return *_frames[frame_id];
    }

    // ====================================================================================================


    // 私有成员============================================================================================
private:
    Device&    _device;
    Swapchain& _swapchain;


    // swapchain 中管理的所有 frame
    std::vector<Frame*> _frames;


    // 当前用于渲染的 frame
    Frame* _current_frame = nullptr;

    // ====================================================================================================
};


}    // namespace Hiss
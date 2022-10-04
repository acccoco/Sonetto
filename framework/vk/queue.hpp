#pragma once
#include "utils/tools.hpp"


namespace Hiss
{
enum class QueueFlag
{
    Graphics,
    Present,
    Compute,
    Transfer,
    AllPowerful,    // 全能
};


struct Queue
{
    Queue(vk::Queue queue, uint32_t family_index, QueueFlag queue_flag)
        : vkqueue(queue),
          queue_family_index(family_index),
          queue_flag(queue_flag)
    {}


    // 两个 queue 的 queue family 是否相同
    static bool is_same_queue_family(const Queue& q1, const Queue& q2)
    {
        return q1.queue_family_index._value == q2.queue_family_index._value;
    }


    // 提交命令执行
    void submit_commands(const std::vector<StageSemaphore>& dst, const std::vector<vk::CommandBuffer>& command_buffers,
                const std::vector<vk::Semaphore>& signal_semaphores, vk::Fence fence = VK_NULL_HANDLE)
    {
        std::vector<vk::PipelineStageFlags> stages(dst.size());
        std::vector<vk::Semaphore>          wait_semaphores(dst.size());
        for (int i = 0; i < dst.size(); ++i)
        {
            stages[i]          = dst[i].stage;
            wait_semaphores[i] = dst[i].semaphore;
        }

        vk::SubmitInfo submit_info = {
                .waitSemaphoreCount   = static_cast<uint32_t>(dst.size()),
                .pWaitSemaphores      = wait_semaphores.data(),
                .pWaitDstStageMask    = stages.data(),
                .commandBufferCount   = static_cast<uint32_t>(command_buffers.size()),
                .pCommandBuffers      = command_buffers.data(),
                .signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size()),
                .pSignalSemaphores    = signal_semaphores.data(),
        };

        if (fence)
            vkqueue._value.submit({submit_info}, fence);
        else
            vkqueue._value.submit({submit_info});
    }


#pragma region public properties
public:
    Prop<vk::Queue, Queue> vkqueue;
    Prop<uint32_t, Queue>  queue_family_index;
    Prop<QueueFlag, Queue> queue_flag;

#pragma endregion
};
}    // namespace Hiss
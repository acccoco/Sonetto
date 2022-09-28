#pragma once

namespace Hiss
{
enum class QueueFlag
{
    Graphics,
    Present,
    Compute,
    Transfer,
    AllPowerful,
};


struct Queue
{
    vk::Queue queue        = VK_NULL_HANDLE;
    uint32_t  family_index = {};
    QueueFlag flag         = {};


    static bool is_same_queue_family(const Queue& q1, const Queue& q2)
    {
        return q1.family_index == q2.family_index;
    }
};
}    // namespace Hiss
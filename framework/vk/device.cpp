#include "vk/device.hpp"
#include "window.hpp"
#include "utils/tools.hpp"
#include "vk/vk_config.hpp"
#include <set>


Hiss::Device::Device(GPU& physical_device_)
    : _gpu(physical_device_)
{
    create_logical_device();
    create_command_pool();
    fence_pool = new FencePool(*this);
}


void Hiss::Device::create_logical_device()
{
    float queue_priority = 1.f;
    /* queue 的创建信息 */
    vk::DeviceQueueCreateInfo queue_info = {
            .queueFamilyIndex = _gpu.queue_family_index(),
            .queueCount       = 1,
            .pQueuePriorities = &queue_priority,
    };


    /* extensions */
    std::vector<const char*> device_ext_list = get_device_extensions();


    /* feature */
    vk::PhysicalDeviceFeatures device_feature = get_device_features();

    // dynamic rendering 需要在 .pNext 字段添加
    vk::PhysicalDeviceDynamicRenderingFeatures feature = {.dynamicRendering = VK_TRUE};

    vkdevice = _gpu.vkgpu.get().createDevice(vk::DeviceCreateInfo{
            .pNext                   = &feature,
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &queue_info,
            .enabledExtensionCount   = (uint32_t) device_ext_list.size(),
            .ppEnabledExtensionNames = device_ext_list.data(),
            .pEnabledFeatures        = &device_feature,
    });


    /* 获取 queue */
    queue = {
            .queue        = vkdevice().getQueue(_gpu.queue_family_index(), 0),
            .family_index = _gpu.queue_family_index(),
            .flag         = QueueFlag::AllPowerful,
    };

    spdlog::info("queue family index: {}", queue().family_index);


    this->set_debug_name(vk::ObjectType::eQueue, (VkQueue) queue().queue, "all powerfu queue");
}


void Hiss::Device::create_command_pool()
{
    command_pool = new CommandPool(*this, queue._value);

    this->set_debug_name(vk::ObjectType::eCommandPool, (VkCommandPool) command_pool().pool_get(),
                         "default command pool");
}


Hiss::Device::~Device()
{
    DELETE(fence_pool._ptr);
    DELETE(command_pool._ptr);
    vkdevice().destroy();
}


vk::DeviceMemory Hiss::Device::allocate_memory(const vk::MemoryRequirements&  mem_require,
                                               const vk::MemoryPropertyFlags& mem_prop) const
{
    auto device_memory_properties = _gpu.vkgpu.get().getMemoryProperties();

    /* 根据 mem require 和 properties 在 device 中找到合适的 memory type，获得其 index */
    std::optional<uint32_t> mem_idx;
    for (uint32_t i = 0; i < device_memory_properties.memoryTypeCount; ++i)
    {
        if (!(mem_require.memoryTypeBits & (1 << i)))
            continue;
        if (!BITS_CONTAIN(device_memory_properties.memoryTypes[i].propertyFlags, mem_prop))
            continue;
        mem_idx = i;
    }
    if (!mem_idx.has_value())
        throw std::runtime_error("no proper memory type for buffer, didn't allocate buffer.");

    return vkdevice().allocateMemory({
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = mem_idx.value(),
    });
}


vk::Semaphore Hiss::Device::create_semaphore(bool signal)
{
    assert(fence_pool._ptr);

    vk::Semaphore semaphore = vkdevice().createSemaphore({});
    if (!signal)
        return semaphore;

    /* 提交一个空命令，并通知刚创建的 semaphore，这样来创建 signaled 状态的 semphore */
    vk::Fence temp_fence = fence_pool().acquire(false);
    queue().queue.submit(vk::SubmitInfo{.signalSemaphoreCount = 1, .pSignalSemaphores = &semaphore}, temp_fence);
    if (vk::Result::eSuccess != vkdevice().waitForFences({temp_fence}, VK_TRUE, UINT64_MAX))
        throw std::runtime_error("error on create semaphore.");
    fence_pool().revert(temp_fence);
    return semaphore;
}
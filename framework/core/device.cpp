#include "device.hpp"
#include "core/window.hpp"
#include "utils/tools.hpp"
#include "vk_config.hpp"
#include <set>


Hiss::Device::Device(GPU& physical_device_)
    : _gpu(physical_device_)
{
    create_logical_device();
    create_command_pool();
    _fence_pool = new FencePool(*this);
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

    vkdevice = _gpu.vkgpu().createDevice(vk::DeviceCreateInfo{
            .pNext                   = &feature,
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &queue_info,
            .enabledExtensionCount   = (uint32_t) device_ext_list.size(),
            .ppEnabledExtensionNames = device_ext_list.data(),
            .pEnabledFeatures        = &device_feature,
    });


    /* 获取 queue */
    _queue = new Queue(vkdevice._value.getQueue(_gpu.queue_family_index(), 0), _gpu.queue_family_index(),
                       QueueFlag::AllPowerful);


    spdlog::info("[device] queue family index: {}", queue().queue_family_index());


    this->set_debug_name(vk::ObjectType::eQueue, (VkQueue) queue().vkqueue(), "all powerfu queue");
}


void Hiss::Device::create_command_pool()
{
    _command_pool = new CommandPool(*this, *_queue);

    this->set_debug_name(vk::ObjectType::eCommandPool, (VkCommandPool) _command_pool->vkpool(),
                         "default command pool");
}


Hiss::Device::~Device()
{
    DELETE(_fence_pool);
    DELETE(_command_pool);
    DELETE(_queue);
    vkdevice().destroy();
}


vk::DeviceMemory Hiss::Device::allocate_memory(const vk::MemoryRequirements&  mem_require,
                                               const vk::MemoryPropertyFlags& mem_prop) const
{
    auto device_memory_properties = _gpu.vkgpu().getMemoryProperties();

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
    assert(_fence_pool);

    vk::Semaphore semaphore = vkdevice().createSemaphore({});
    if (!signal)
        return semaphore;

    /* 提交一个空命令，并通知刚创建的 semaphore，这样来创建 signaled 状态的 semphore */
    vk::Fence temp_fence = fence_pool().acquire(false);
    queue().vkqueue().submit(vk::SubmitInfo{.signalSemaphoreCount = 1, .pSignalSemaphores = &semaphore}, temp_fence);
    if (vk::Result::eSuccess != vkdevice().waitForFences({temp_fence}, VK_TRUE, UINT64_MAX))
        throw std::runtime_error("error on create semaphore.");
    fence_pool().revert(temp_fence);
    return semaphore;
}
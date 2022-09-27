#include "vk/device.hpp"
#include "window.hpp"
#include "utils/tools.hpp"
#include "vk/vk_config.hpp"
#include <set>


Hiss::Device::Device(GPU& physical_device_, spdlog::logger& logger)
    : _gpu(physical_device_),
      _logger(logger)
{
    logical_device_create();
    command_pool_create();
    _fence_pool = new FencePool(*this);
}

// TODO 动态地申请 queue，可以再最后一级地 sample class 中再申请 queue，因此将申请过程放到 prepare 里面
void Hiss::Device::logical_device_create()
{
    /* queue 的创建信息 */
    std::vector<vk::DeviceQueueCreateInfo> queue_info;
    float                                  queue_priority = 1.f;

    auto graphics_queue_index = _gpu.graphics_queue_index();
    auto present_queue_index  = _gpu.present_queue_index();
    auto compute_queue_index  = _gpu.compute_queue_index();
    if (!graphics_queue_index.has_value() || !present_queue_index.has_value() || !compute_queue_index.has_value())
        throw std::runtime_error("no queue.");
    for (uint32_t queue_family_idx:
         std::set<uint32_t>{graphics_queue_index.value(), present_queue_index.value(), compute_queue_index.value()})
    {
        queue_info.push_back(vk::DeviceQueueCreateInfo{.queueFamilyIndex = queue_family_idx,
                                                       .queueCount       = 1,
                                                       .pQueuePriorities = &queue_priority});
    }


    /* extensions */
    std::vector<const char*> device_ext_list = get_device_extensions();


    /* feature */
    vk::PhysicalDeviceFeatures device_feature = get_device_features();

    vk::PhysicalDeviceDynamicRenderingFeatures feature = {.dynamicRendering = VK_TRUE};

    _device = _gpu.handle_get().createDevice(vk::DeviceCreateInfo{
            .pNext                   = &feature,
            .queueCreateInfoCount    = (uint32_t) queue_info.size(),
            .pQueueCreateInfos       = queue_info.data(),
            .enabledExtensionCount   = (uint32_t) device_ext_list.size(),
            .ppEnabledExtensionNames = device_ext_list.data(),
            .pEnabledFeatures        = &device_feature,
    });


    /* 获取 queue */
    _queue_graphics = {
            .queue        = _device.getQueue(graphics_queue_index.value(), 0),
            .family_index = graphics_queue_index.value(),
            .flag         = QueueFlag::Graphics,
    };
    _queue_present = {
            .queue        = _device.getQueue(present_queue_index.value(), 0),
            .family_index = present_queue_index.value(),
            .flag         = QueueFlag::Present,
    };
    _queue_compute = {
            .queue        = _device.getQueue(compute_queue_index.value(), 0),
            .family_index = compute_queue_index.value(),
            .flag         = QueueFlag::Compute,
    };
    _logger.info("[Device] queue graphics family index: {}", _queue_graphics.family_index);
    _logger.info("[Device] queue present family index: {}", _queue_present.family_index);
    _logger.info("[Device] queue compute family index: {}", _queue_compute.family_index);

    this->set_debug_name(vk::ObjectType::eQueue, (VkQueue) _queue_graphics.queue, "graphics_queue");
    this->set_debug_name(vk::ObjectType::eQueue, (VkQueue) _queue_compute.queue, "compute_queue");
    this->set_debug_name(vk::ObjectType::eQueue, (VkQueue) _queue_present.queue, "present_queue");
}


void Hiss::Device::command_pool_create()
{
    _command_pool_graphics = new CommandPool(*this, _queue_graphics);
    _command_pool_present  = new CommandPool(*this, _queue_present);
    _command_pool_compute  = new CommandPool(*this, _queue_compute);

    this->set_debug_name(vk::ObjectType::eCommandPool, (VkCommandPool) _command_pool_graphics->pool_get(),
                         "graphcis_command_pool");
    this->set_debug_name(vk::ObjectType::eCommandPool, (VkCommandPool) _command_pool_compute->pool_get(),
                         "compute_command_pool");
    this->set_debug_name(vk::ObjectType::eCommandPool, (VkCommandPool) _command_pool_present->pool_get(),
                         "present_command_pool");
}


Hiss::Device::~Device()
{
    DELETE(_fence_pool);
    DELETE(_command_pool_graphics);
    DELETE(_command_pool_present);
    DELETE(_command_pool_compute);
    _device.destroy();
}


vk::DeviceMemory Hiss::Device::memory_allocate(const vk::MemoryRequirements&  mem_require,
                                               const vk::MemoryPropertyFlags& mem_prop) const
{
    auto device_memory_properties = _gpu.handle_get().getMemoryProperties();

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

    return _device.allocateMemory({
            .allocationSize  = mem_require.size,
            .memoryTypeIndex = mem_idx.value(),
    });
}


vk::Semaphore Hiss::Device::semaphore_create(bool signal)
{
    assert(_fence_pool);

    vk::Semaphore semaphore = _device.createSemaphore({});
    if (!signal)
        return semaphore;

    /* 提交一个空命令，并通知刚创建的 semaphore，这样来创建 signaled 状态的 semphore */
    vk::Fence temp_fence = _fence_pool->acquire(false);
    _queue_compute.queue.submit(vk::SubmitInfo{.signalSemaphoreCount = 1, .pSignalSemaphores = &semaphore}, temp_fence);
    if (vk::Result::eSuccess != _device.waitForFences({temp_fence}, VK_TRUE, UINT64_MAX))
        throw std::runtime_error("error on create semaphore.");
    _fence_pool->revert(temp_fence);
    return semaphore;
}
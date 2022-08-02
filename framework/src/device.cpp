#include "device.hpp"
#include "window.hpp"
#include "tools.hpp"
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
    std::vector<const char*> device_ext_list = {
            /* 这是一个临时的扩展（vulkan_beta.h)，在 metal API 上模拟 vulkan 需要这个扩展 */
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
            /* 可以将渲染结果呈现到 window surface 上 */
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };


    /* feature */
    [[maybe_unused]] vk::PhysicalDeviceFeatures device_feature{
            .tessellationShader = VK_TRUE,
            .sampleRateShading  = VK_TRUE,
            .samplerAnisotropy  = VK_TRUE,
    };


    _device = _gpu.handle_get().createDevice(vk::DeviceCreateInfo{
            .queueCreateInfoCount    = (uint32_t) queue_info.size(),
            .pQueueCreateInfos       = queue_info.data(),
            .enabledExtensionCount   = (uint32_t) device_ext_list.size(),
            .ppEnabledExtensionNames = device_ext_list.data(),
            .pEnabledFeatures        = &device_feature,
    });


    /* 获取 queue */
    _graphics_queue = {
            .queue        = _device.getQueue(graphics_queue_index.value(), 0),
            .family_index = graphics_queue_index.value(),
            .flag         = QueueFlag::Graphics,
    };
    _present_queue = {
            .queue        = _device.getQueue(present_queue_index.value(), 0),
            .family_index = present_queue_index.value(),
            .flag         = QueueFlag::Present,
    };
    _compute_queue = {
            .queue        = _device.getQueue(compute_queue_index.value(), 0),
            .family_index = compute_queue_index.value(),
            .flag         = QueueFlag::Compute,
    };
}


void Hiss::Device::command_pool_create()
{
    _graphics_command_pool = new CommandPool(*this, _graphics_queue);
    _present_command_pool  = new CommandPool(*this, _present_queue);
    _compute_command_pool  = new CommandPool(*this, _compute_queue);
}


Hiss::Device::~Device()
{
    DELETE(_fence_pool);
    DELETE(_graphics_command_pool);
    DELETE(_present_command_pool);
    DELETE(_compute_command_pool);
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

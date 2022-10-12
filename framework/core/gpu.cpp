#include "gpu.hpp"


Hiss::GPU::GPU(vk::PhysicalDevice physical_device, vk::SurfaceKHR surface)
    : vkgpu(physical_device),
      properties(physical_device.getProperties()),
      features(physical_device.getFeatures()),
      memory_properties(physical_device.getMemoryProperties())
{
    // 找到全能的队列
    auto all_powerful_queue = Hiss::GPU::find_all_powerful_queue(physical_device, surface);
    if (!all_powerful_queue.has_value())
        throw std::runtime_error("no all-powerful queue family found.");
    else
        queue_family_index = all_powerful_queue.value();


    // 找到合适的 depth format
    auto format = filter_format(
            {
                    vk::Format::eD32Sfloat,
                    vk::Format::eD32SfloatS8Uint,
                    vk::Format::eD24UnormS8Uint,
            },
            vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    if (!format.has_value())
        throw std::runtime_error("no suitable depth format found.");
    this->depth_stencil_format._value = format.value();


    // 最大的 mass 采样数
    max_msaa_cnt._value = max_sample_cnt();
}


std::optional<vk::Format> Hiss::GPU::filter_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                                   vk::FormatFeatureFlags features_) const
{
    for (const vk::Format& format: candidates)
    {
        vk::FormatProperties props = vkgpu().getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && BITS_CONTAIN(props.linearTilingFeatures, features_))
            return format;
        if (tiling == vk::ImageTiling::eOptimal && BITS_CONTAIN(props.optimalTilingFeatures, features_))
            return format;
    }
    return std::nullopt;
}


/**
 * 找到 device 允许的最大 MSAA 数
 */
vk::SampleCountFlagBits Hiss::GPU::max_sample_cnt() const
{
    auto physical_device_properties = vkgpu().getProperties();

    /* 在 color 和 depth 都支持的 sample count 中，取最大值 */
    vk::SampleCountFlags counts = physical_device_properties.limits.framebufferColorSampleCounts
                                & physical_device_properties.limits.framebufferDepthSampleCounts;
    for (auto bit: {
                 vk::SampleCountFlagBits::e64,
                 vk::SampleCountFlagBits::e32,
                 vk::SampleCountFlagBits::e16,
                 vk::SampleCountFlagBits::e8,
                 vk::SampleCountFlagBits::e4,
                 vk::SampleCountFlagBits::e2,
         })
        if (counts & bit)
            return bit;
    return vk::SampleCountFlagBits::e1;
}


bool Hiss::GPU::is_support_linear_filter(vk::Format format) const
{
    vk::FormatProperties format_property = vkgpu().getFormatProperties(format);
    return BITS_CONTAIN(format_property.optimalTilingFeatures, vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
}


std::optional<uint32_t> Hiss::GPU::find_all_powerful_queue(vk::PhysicalDevice gpu, vk::SurfaceKHR surface)
{
    std::optional<uint32_t> all_powerful_queue;

    auto queue_properties = gpu.getQueueFamilyProperties();
    for (uint32_t queue_index = 0; queue_index < queue_properties.size(); ++queue_index)
    {
        if (!(queue_properties[queue_index].queueFlags & vk::QueueFlagBits::eGraphics))
            continue;
        if (!(queue_properties[queue_index].queueFlags & vk::QueueFlagBits::eCompute))
            continue;
        if (!gpu.getSurfaceSupportKHR(queue_index, surface))
            continue;
        all_powerful_queue = queue_index;
        break;
    }
    return all_powerful_queue;
}

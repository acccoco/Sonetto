#include "physical_device.hpp"


Hiss::GPU::GPU(vk::PhysicalDevice physical_device, vk::SurfaceKHR surface)
    : _handle(physical_device),
      _features(physical_device.getFeatures()),
      _properties(physical_device.getProperties()),
      _memory_properties(physical_device.getMemoryProperties())
{
    auto queue_properties = _handle.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queue_properties.size(); ++i)
    {
        if (queue_properties[i].queueFlags & vk::QueueFlagBits::eGraphics)
            _graphics_queue_family_index.push_back(i);
        if (queue_properties[i].queueFlags & vk::QueueFlagBits::eCompute)
            _compute_queue_family_index.push_back(i);
        if (physical_device.getSurfaceSupportKHR(i, surface))
            _present_queue_family_index.push_back(i);
    }
}


std::optional<vk::Format> Hiss::GPU::format_filter(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                                   vk::FormatFeatureFlags features) const
{
    for (const vk::Format& format: candidates)
    {
        vk::FormatProperties props = _handle.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && BITS_CONTAIN(props.linearTilingFeatures, features))
            return format;
        if (tiling == vk::ImageTiling::eOptimal && BITS_CONTAIN(props.optimalTilingFeatures, features))
            return format;
    }
    return std::nullopt;
}


/**
 * 找到 device 允许的最大 MSAA 数
 */
vk::SampleCountFlagBits Hiss::GPU::max_sample_cnt()
{
    auto physical_device_properties = _handle.getProperties();

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


std::optional<uint32_t> Hiss::GPU::graphics_queue_index() const
{
    // NOTE 这里确保了 queue family index 不同
    if (_graphics_queue_family_index.empty())
        return std::nullopt;
    return _graphics_queue_family_index.front();
}


std::optional<uint32_t> Hiss::GPU::present_queue_index() const
{
    if (_present_queue_family_index.empty())
        return std::nullopt;
    return _present_queue_family_index.back();
}


std::optional<uint32_t> Hiss::GPU::compute_queue_index() const
{
    if (_compute_queue_family_index.empty())
        return std::nullopt;
    if (_compute_queue_family_index.size() > 2)
        return _compute_queue_family_index[1];
    return _compute_queue_family_index.front();
}


bool Hiss::GPU::format_support_linear_filter(vk::Format format) const
{
    vk::FormatProperties format_property = _handle.getFormatProperties(format);
    return BITS_CONTAIN(format_property.optimalTilingFeatures, vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
}
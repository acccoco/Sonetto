#pragma once
#include "utils/tools.hpp"
#include "vk/vk_common.hpp"

namespace Hiss
{
class GPU
{
public:
    GPU(vk::PhysicalDevice physical_device, vk::SurfaceKHR surface);
    ~GPU() = default;

    [[nodiscard]] std::optional<uint32_t> graphics_queue_index() const;
    [[nodiscard]] std::optional<uint32_t> present_queue_index() const;
    [[nodiscard]] std::optional<uint32_t> compute_queue_index() const;

    [[nodiscard]] uint32_t get_queue_family_index() const
    {
        return _queue_family_index;
    }

    [[nodiscard]] vk::PhysicalDeviceProperties properties() const
    {
        return _properties;
    }

    [[nodiscard]] vk::PhysicalDevice handle_get() const
    {
        return _handle;
    }

    /// 根据 tiling 和 features，在 candidate 中找到合适的 format
    [[nodiscard]] std::optional<vk::Format> format_filter(const std::vector<vk::Format>& candidates,
                                                          vk::ImageTiling                tiling,
                                                          vk::FormatFeatureFlags         features) const;

    /// gpu 支持的最大 MSAA 采样数
    vk::SampleCountFlagBits max_sample_cnt();

    /// format 是否支持 linear filter
    [[nodiscard]] bool is_support_linear_filter(vk::Format format) const;

public:
    Prop<vk::PhysicalDevice, GPU> acc{VK_NULL_HANDLE};
    Prop<vk::Format, GPU>         depth_stencil_format{};

private:
    const uint32_t EXPECT_GRAPHICS_QUEUE_INDEX = 0;
    const uint32_t EXPECT_PRESENT_QUEUE_INDEX  = 0;
    const uint32_t EXPECT_COMPUTE_QUEUE_INDEX  = 0;

    vk::PhysicalDevice                 _handle                      = VK_NULL_HANDLE;
    vk::PhysicalDeviceFeatures         _features                    = {};
    vk::PhysicalDeviceProperties       _properties                  = {};
    vk::PhysicalDeviceMemoryProperties _memory_properties           = {};
    uint32_t                           _queue_family_index          = {};
    std::vector<uint32_t>              _graphics_queue_family_index = {};
    std::vector<uint32_t>              _present_queue_family_index  = {};
    std::vector<uint32_t>              _compute_queue_family_index  = {};
};
}    // namespace Hiss
#pragma once
#include "utils/tools.hpp"
#include "vk_common.hpp"

namespace Hiss
{
class GPU
{
public:
    GPU(vk::PhysicalDevice physical_device, vk::SurfaceKHR surface);
    ~GPU() = default;


#pragma region 工具方法
public:
    /// format 是否支持 linear filter
    [[nodiscard]] bool is_support_linear_filter(vk::Format format) const;
#pragma endregion


#pragma region 数据初始化的方法
private:
    // 找到全能的队列：graphics，present，compute
    static std::optional<uint32_t> find_all_powerful_queue(vk::PhysicalDevice gpu, vk::SurfaceKHR surface);


    /// gpu 支持的最大 MSAA 采样数
    [[nodiscard]] vk::SampleCountFlagBits max_sample_cnt() const;


    /// 根据 tiling 和 features，在 candidate 中找到合适的 format
    [[nodiscard]] std::optional<vk::Format> filter_format(const std::vector<vk::Format>& candidates,
                                                          vk::ImageTiling                tiling,
                                                          vk::FormatFeatureFlags         features_) const;
#pragma endregion


#pragma region 公共属性
public:
    Prop<vk::Format, GPU>                         depth_stencil_format{};
    Prop<vk::SampleCountFlagBits, GPU>            max_msaa_cnt{};
    Prop<vk::PhysicalDevice, GPU>                 vkgpu{VK_NULL_HANDLE};
    Prop<vk::PhysicalDeviceProperties, GPU>       properties{};
    Prop<uint32_t, GPU>                           queue_family_index{};
    Prop<vk::PhysicalDeviceFeatures, GPU>         features{};
    Prop<vk::PhysicalDeviceMemoryProperties, GPU> memory_properties{};
#pragma endregion
};
}    // namespace Hiss
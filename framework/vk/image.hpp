#pragma once
#include "vk/buffer.hpp"
#include "vk/device.hpp"


namespace Hiss
{

class Image2D
{
public:
    struct Info
    {
        std::string              name;
        vk::Format               format     = {};
        vk::Extent2D             extent     = {};
        uint32_t                 mip_levels = 1;
        vk::ImageUsageFlags      usage      = {};
        vk::ImageTiling          tiling     = vk::ImageTiling::eOptimal;
        vk::SampleCountFlagBits  samples    = vk::SampleCountFlagBits::e1;
        VmaAllocationCreateFlags memory_flags;
        vk::ImageAspectFlags     aspect;
        vk::ImageLayout          init_layout = vk::ImageLayout::eUndefined;
    };

    struct View
    {
        vk::ImageView             vkview;
        vk::ImageSubresourceRange range;
    };


    Image2D(VmaAllocator allocator, Device& device, const Info& info);

    // 外部已经创建好了 image
    Image2D(Device& device, vk::Image image, const std::string& name, vk::ImageAspectFlags aspect,
            vk::ImageLayout layout, vk::Format format, vk::Extent2D extent);


    ~Image2D();


    // 访问某个 image view
    View view(uint32_t base_mip = 0, uint32_t mip_count = 1);


    // 立刻执行 layout transfer，会检查连续的相同 layout，减少 gpu 的调用次数
    void transfer_layout_im(vk::ImageLayout new_layout, uint32_t base_level = 0, uint32_t level_count = 1);


    // execution image memory barrier
    void execution_barrier(const StageAccess& src, const StageAccess& dst, vk::CommandBuffer command_buffer,
                           uint32_t base_level = 0, uint32_t level_count = 1);


    /**
     * 向 command buffer 中添加 layout transfer 的指令
     * @param clear 是否清除原数据
     */
    void transfer_layout(std::optional<StageAccess> src, std::optional<StageAccess> dst,
                         vk::CommandBuffer command_buffer, vk::ImageLayout new_layout, bool clear = false,
                         uint32_t base_level = 0, uint32_t level_count = 1);

private:
    // 创建 image view
    View create_view(uint32_t base_mip, uint32_t mip_count);


    // 立刻执行 layout transfer
    void transfer_layout_im(vk::ImageLayout old_layout, vk::ImageLayout new_layout, uint32_t base_mip,
                            uint32_t mip_count);


public:
    Prop<VkImage, Image2D>              vkimage{};
    Prop<std::string, Image2D>          name{};
    Prop<vk::Format, Image2D>           format{};
    Prop<vk::Extent2D, Image2D>         extent{};
    Prop<vk::ImageAspectFlags, Image2D> aspect{};
    Prop<uint32_t, Image2D>             mip_levels{};


private:
    Device& _device;

    VmaAllocator  _allocator  = nullptr;
    VmaAllocation _allocation = nullptr;

    bool is_proxy = false;    // image 来自类的外部，并非在类中创建

    std::vector<View> _views;    // view 主要用于 mip levels

    std::vector<vk::ImageLayout> _layouts;    // 每个 mip 层级的 layout 信息
};
}    // namespace Hiss
#pragma once
#include "vk/buffer.hpp"
#include "vk/device.hpp"


namespace Hiss
{


class Image
{
public:
    struct CreateInfo
    {
        Device&                 device;
        vk::Format              format            = {};
        vk::Extent2D            extent            = {};
        uint32_t                mip_levels        = 1;
        std::vector<uint32_t>   sharing_queues    = {};
        vk::ImageUsageFlags     usage             = {};
        vk::MemoryPropertyFlags memory_properties = {};
        vk::ImageTiling         tiling            = vk::ImageTiling::eOptimal;
        vk::SampleCountFlagBits samples           = vk::SampleCountFlagBits::e1;
    };

    explicit Image(CreateInfo&& create_info);
    Image(Image&)  = delete;
    Image(Image&&) = delete;
    ~Image();

    [[nodiscard]] Device& device_get() const
    {
        return _device;
    }
    [[nodiscard]] vk::Format format_get() const
    {
        return _format;
    }
    [[nodiscard]] vk::SampleCountFlagBits samples_get() const
    {
        return _samples;
    }
    [[nodiscard]] vk::Image vkimage() const
    {
        return _image;
    }
    [[nodiscard]] uint32_t mip_levels() const
    {
        return _mip_levels;
    }

    /**
     * 默认 old layout = undefined，使用 graphics queue 进行转换
     */
    void layout_tran(vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::ImageAspectFlags image_aspect,
                     uint32_t base_mip_level, uint32_t mip_level_count);

    /**
     * 将 buffer 数据传入 image 的 level#0，会将 image level#0 变为 TransferDst layout，使用 graphics queue 进行转换
     */
    void copy_buffer_to_image(vk::Buffer buffer, vk::ImageAspectFlags aspect);


    // 创建 depth，并立即转换为 eDepthStencilAttachmentOptimal layout
    static Image* create_depth_attach(Device& device, vk::Extent2D extent, const std::string& name)
    {
        vk::Format depth_format = device.get_gpu().depth_stencil_format.get();
        auto       depth_image  = new Image(Image::CreateInfo{
                       .device            = device,
                       .format            = depth_format,
                       .extent            = extent,
                       .usage             = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                       .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });
        device.set_debug_name(vk::ObjectType::eImage, (VkImage) depth_image->vkimage(), name);
        depth_image->layout_tran(vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                 vk::ImageAspectFlagBits::eDepth, 0, 1);

        return depth_image;
    }


    /**
     * 注：
     *  - 确保所有 level 都是 transfer_dst layout
     *  - 使用 graphics queue 进行创建
     *  - 会将 layout 转换为 readonly
     * @return 如果 format 不支持 linear filter，则无法创建 mipmap
     */
    bool mipmap_generate(vk::ImageAspectFlags aspect);


private:
    Image(Device& device, vk::Format format, const vk::Extent2D& extent, uint32_t mip_levels,
          const std::vector<uint32_t>& sharing_queues, vk::ImageUsageFlags usage,
          vk::MemoryPropertyFlags memory_properties, vk::ImageTiling tiling, vk::SampleCountFlagBits samples);

    // members =======================================================

private:
    Device&                 _device;
    vk::Image               _image      = {};
    vk::DeviceMemory        _memory     = {};
    vk::Extent2D            _extent     = {};
    vk::Format              _format     = {};
    uint32_t                _mip_levels = {};
    vk::ImageTiling         _tiling     = {};
    vk::ImageUsageFlags     _usage      = {};
    vk::SampleCountFlagBits _samples    = {};
};


class ImageView
{
public:
    ImageView(const Image& image, vk::ImageAspectFlags image_aspect, uint32_t base_mip_level, uint32_t n_mip_level);
    ImageView(ImageView&)        = delete;
    ImageView(ImageView&& other) = delete;
    ~ImageView();

    [[nodiscard]] vk::ImageView view_get() const
    {
        return _image_view;
    }
    [[nodiscard]] const vk::ImageSubresourceRange& subresource_range() const
    {
        return _subresource;
    }


private:
    Device&             _device;
    vk::ImageView             _image_view  = VK_NULL_HANDLE;
    vk::Format                _format      = {};
    vk::ImageSubresourceRange _subresource = {};
};
}    // namespace Hiss
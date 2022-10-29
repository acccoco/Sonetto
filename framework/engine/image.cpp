#include <fmt/format.h>
#include "image.hpp"


void Hiss::Image2D::copy_buffer_to_image(vk::Buffer buffer)
{
    vk::BufferImageCopy copy_info = {
            .bufferOffset = 0,
            /* zero for tightly packed */
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,

            .imageSubresource = vk::ImageSubresourceLayers{.aspectMask     = aspect._value,
                                                           .mipLevel       = 0,
                                                           .baseArrayLayer = 0,
                                                           .layerCount     = 1},
            .imageOffset      = {0, 0, 0},
            .imageExtent      = {extent._value.width, extent._value.height, 1},
    };


    Hiss::OneTimeCommand command(_device, _device.command_pool());
    command().copyBufferToImage(buffer, vkimage._value, vk::ImageLayout::eTransferDstOptimal, {copy_info});
    command.exec();
}


/**
 * 注：
 *  - 确保所有 level 的 layout 都是 transfer_dst
 *
 * 生成方法：
 *  - loop(i: 1 -> n-1)
 *      |_ layout transition(level#i): transfer_dst -> transfer_src
 *      |_ blit: level#i -> level#i+1(layout: transfer_dst)
 *      |_ layout transition(level#i): tranfer_src -> shader_read_only
 *  - layout transition(level#n): transfer_dst -> shader_read_only
 */
//bool Hiss::Image::mipmap_generate(vk::ImageAspectFlags aspect)
//{
//    assert(_mip_levels > 0);
//    if (_mip_levels == 1)
//        return true;
//
//    if (!_device.gpu().is_support_linear_filter(_format))
//        return false;
//
//
//    Hiss::OneTimeCommand   command(_device, _device.command_pool());
//    vk::ImageMemoryBarrier barrier = {
//            .image            = _image,
//            .subresourceRange = {.aspectMask = aspect, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
//
//
//    for (struct {
//             int32_t  mip_width;
//             int32_t  mip_height;
//             uint32_t level;
//         } _{(int32_t) _extent.width, (int32_t) _extent.height, 0};
//         _.level < _mip_levels - 1; ++_.level)
//    {
//        /* layout transition(level#i): transfer_dst -> transfer_src */
//        barrier.subresourceRange.baseMipLevel = _.level;
//        barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
//        barrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
//        barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
//        barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferRead;
//        command().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {},
//                                  {}, {barrier});
//
//
//        /**
//         * blit: level#i -> level#i+1
//         * dst 的 layout 只允许 3 种，这里 transfer_dst 最好
//         */
//        auto src_offset = std::array<vk::Offset3D, 2>{
//                vk::Offset3D{0, 0, 0},
//                vk::Offset3D{_.mip_width, _.mip_height, 1},
//        };
//        auto dst_offset = std::array<vk::Offset3D, 2>{
//                vk::Offset3D{0, 0, 0},
//                vk::Offset3D{std::max(1, _.mip_width / 2), std::max(1, _.mip_height / 2), 1},
//        };
//        vk::ImageBlit blit = {
//                .srcSubresource = {.aspectMask = aspect, .mipLevel = _.level, .baseArrayLayer = 0, .layerCount = 1},
//                .srcOffsets     = src_offset,
//                .dstSubresource = {.aspectMask = aspect, .mipLevel = _.level + 1, .baseArrayLayer = 0, .layerCount = 1},
//                .dstOffsets     = dst_offset,
//        };
//        command().blitImage(_image, vk::ImageLayout::eTransferSrcOptimal, _image, vk::ImageLayout::eTransferDstOptimal,
//                            {blit}, vk::Filter::eLinear);
//
//
//        /* layout transition(level#i): transfer_src -> shader_readonly */
//        barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
//        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
//        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
//        barrier.dstAccessMask = {};    // 这这是个 execution barrier
//        command().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {},
//                                  {}, {}, {barrier});
//
//
//        _.mip_width  = _.mip_width > 1 ? _.mip_width / 2 : _.mip_width;
//        _.mip_height = _.mip_height > 1 ? _.mip_height / 2 : _.mip_height;
//    }
//
//
//    /* layout transition(level#n): transfer_dst -> shader_readonly */
//    barrier.subresourceRange.baseMipLevel = _mip_levels - 1;
//    barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
//    barrier.newLayout                     = vk::ImageLayout::eShaderReadOnlyOptimal;
//    barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
//    barrier.dstAccessMask                 = {};    // execution barrier
//    command().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {},
//                              {}, {barrier});
//
//
//    command.exec();
//    return true;
//}


Hiss::Image2D::Image2D(VmaAllocator allocator, Hiss::Device& device, const Hiss::Image2DCreateInfo& info)
    : name(info.name),
      format(info.format),
      extent(info.extent),
      aspect(info.aspect),
      _device(device),
      _allocator(allocator),
      _layout(vk::ImageLayout::eUndefined)
{
    VkImageCreateInfo image_info = vk::ImageCreateInfo{
            .imageType     = vk::ImageType::e2D,
            .format        = info.format,
            .extent        = {.width = info.extent.width, .height = info.extent.height, .depth = 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = info.samples,
            .tiling        = info.tiling,
            .usage         = info.usage,
            .sharingMode   = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,    // 这里只能是 undefined
    };

    VmaAllocationCreateInfo alloc_create_info = {
            .flags    = info.memory_flags,
            .usage    = VMA_MEMORY_USAGE_AUTO,
            .priority = 1.f,
    };

    vmaCreateImage(allocator, &image_info, &alloc_create_info, &vkimage._value, &_allocation, &_alloc_info);
    if (!info.name.empty())
        _device.set_debug_name(vk::ObjectType::eImage, vkimage._value, info.name);


    // 进行 layout 转换
    if (info.init_layout != vk::ImageLayout::eUndefined)
        transfer_layout_im(info.init_layout);

    // 创建基础的 view
    _create_view();
}


Hiss::Image2D::Image2D(Hiss::Device& device, vk::Image image, const std::string& name, vk::ImageAspectFlags aspect,
                       vk::ImageLayout layout, vk::Format format, vk::Extent2D extent)
    : vkimage(image),
      name(name),
      format(format),
      extent(extent),
      aspect(aspect),
      _device(device),
      is_proxy(true),
      _layout(layout)
{
    device.set_debug_name(vk::ObjectType::eImage, VkImage(image), name);
    _create_view();
}


Hiss::Image2D::~Image2D()
{
    if (!is_proxy)
        vmaDestroyImage(_allocator, vkimage._value, _allocation);
    _device.vkdevice().destroy(view._value.vkview);
}


void Hiss::Image2D::transfer_layout_im(vk::ImageLayout new_layout)
{

    vk::ImageMemoryBarrier barrier = {
            .oldLayout        = _layout,
            .newLayout        = new_layout,
            .image            = vkimage._value,
            .subresourceRange = {.aspectMask     = aspect._value,
                                 .baseMipLevel   = 0,
                                 .levelCount     = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount     = 1},
    };

    OneTimeCommand command_buffer{_device, _device.command_pool()};
    command_buffer().pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe,
                                     {}, {}, {}, {barrier});
    command_buffer.exec();


    // 将 layout 的记录更新
    _layout = new_layout;
}


void Hiss::Image2D::execution_barrier(vk::CommandBuffer command_buffer, const StageAccess& src, const StageAccess& dst)
{
    command_buffer.pipelineBarrier(src.stage, dst.stage, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .srcAccessMask    = src.access,
                                           .dstAccessMask    = dst.access,
                                           .oldLayout        = _layout,
                                           .newLayout        = _layout,
                                           .image            = vkimage._value,
                                           .subresourceRange = view().range,
                                   }});
}


void Hiss::Image2D::transfer_layout(vk::CommandBuffer command_buffer, const StageAccess& src, const StageAccess& dst,
                                    vk::ImageLayout new_layout, bool clear)
{
    command_buffer.pipelineBarrier(
            src.stage, dst.stage, {}, {}, {},
            {
                    vk::ImageMemoryBarrier{.srcAccessMask    = src.access,
                                           .dstAccessMask    = dst.access,
                                           .oldLayout        = clear ? vk::ImageLayout::eUndefined : _layout,
                                           .newLayout        = new_layout,
                                           .image            = vkimage._value,
                                           .subresourceRange = view().range},
            });

    // 更新 layout 数据
    _layout = new_layout;
}


void Hiss::Image2D::_create_view()
{
    vk::ImageSubresourceRange range = {
            .aspectMask     = aspect._value,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
    };

    vk::ImageView vkview = _device.vkdevice().createImageView(vk::ImageViewCreateInfo{
            .image            = vkimage._value,
            .viewType         = vk::ImageViewType::e2D,
            .format           = format._value,
            .subresourceRange = range,
    });

    if (!name._value.empty())
        _device.set_debug_name(vk::ObjectType::eImageView, (VkImageView) vkview, fmt::format("{}_view", name._value));

    view._value.vkview = vkview;
    view._value.range  = range;
}


void Hiss::Image2D::memory_barrier(const StageAccess& src, const StageAccess& dst, vk::ImageLayout old_layout,
                                   vk::ImageLayout new_layout, vk::CommandBuffer command_buffer)
{
    command_buffer.pipelineBarrier(src.stage, dst.stage, {}, {}, {},
                                   {
                                           vk::ImageMemoryBarrier{.srcAccessMask    = src.access,
                                                                  .dstAccessMask    = dst.access,
                                                                  .oldLayout        = old_layout,
                                                                  .newLayout        = new_layout,
                                                                  .image            = vkimage._value,
                                                                  .subresourceRange = view().range},
                                   });
}

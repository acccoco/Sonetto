#include <fmt/format.h>
#include "vk/image.hpp"


//void Hiss::Image::copy_buffer_to_image(vk::Buffer buffer, vk::ImageAspectFlags aspect)
//{
//    vk::BufferImageCopy copy_info = {
//            .bufferOffset = 0,
//            /* zero for tightly packed */
//            .bufferRowLength   = 0,
//            .bufferImageHeight = 0,
//
//            .imageSubresource = vk::ImageSubresourceLayers{.aspectMask     = aspect,
//                                                           .mipLevel       = 0,
//                                                           .baseArrayLayer = 0,
//                                                           .layerCount     = 1},
//            .imageOffset      = {0, 0, 0},
//            .imageExtent      = {_extent.width, _extent.height, 1},
//    };
//
//
//    Hiss::OneTimeCommand command(_device, _device.command_pool());
//    command().copyBufferToImage(buffer, _image, vk::ImageLayout::eTransferDstOptimal, {copy_info});
//    command.exec();
//}


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


Hiss::Image2D::Image2D(VmaAllocator allocator, Hiss::Device& device, const Hiss::Image2D::Info& info)
    : name(info.name),
      format(info.format),
      extent(info.extent),
      aspect(info.aspect),
      mip_levels(info.mip_levels),
      _device(device),
      _allocator(allocator)
{
    VkImageCreateInfo image_info = vk::ImageCreateInfo{
            .imageType     = vk::ImageType::e2D,
            .format        = info.format,
            .extent        = {.width = info.extent.width, .height = info.extent.height, .depth = 1},
            .mipLevels     = info.mip_levels,
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

    vmaCreateImage(allocator, &image_info, &alloc_create_info, &vkimage._value, &_allocation, nullptr);
    if (!info.name.empty())
        _device.set_debug_name(vk::ObjectType::eImage, vkimage._value, info.name);

    // 写入 layout 信息
    _layouts = std::vector<vk::ImageLayout>(info.mip_levels, vk::ImageLayout::eUndefined);


    // 进行 layout 转换
    if (info.init_layout != vk::ImageLayout::eUndefined)
        transfer_layout_im(vk::ImageLayout::eUndefined, info.init_layout, 0, info.mip_levels);

    // 创建基础的 view
    _views.push_back(create_view(0, 1));
}


Hiss::Image2D::Image2D(Hiss::Device& device, vk::Image image, const std::string& name, vk::ImageAspectFlags aspect,
                       vk::ImageLayout layout, vk::Format format, vk::Extent2D extent)
    : vkimage(image),
      name(name),
      format(format),
      extent(extent),
      aspect(aspect),
      mip_levels(1),
      _device(device),
      is_proxy(true),
      _layouts({layout})
{
    // 创建基本的 view
    _views.push_back(create_view(0, 1));
}


Hiss::Image2D::~Image2D()
{
    if (!is_proxy)
        vmaDestroyImage(_allocator, vkimage._value, _allocation);
    for (auto& view: _views)
        _device.vkdevice().destroy(view.vkview);
}


Hiss::Image2D::View Hiss::Image2D::view(uint32_t base_mip, uint32_t mip_count)
{
    assert(base_mip + mip_count <= mip_levels._value);

    // 查询是否存在这个 view
    for (auto& view: _views)
    {
        if (view.range.baseMipLevel == base_mip && view.range.levelCount == mip_count)
            return view;
    }

    // 不存在，新创建
    View view = create_view(base_mip, mip_count);
    _views.push_back(view);
    return view;
}


void Hiss::Image2D::transfer_layout_im(vk::ImageLayout new_layout, uint32_t base_level, uint32_t level_count)
{
    assert(base_level + level_count <= mip_levels._value);

    // layout 连续相同的 level 为一组，进行一次 transfer
    uint32_t start_level = base_level;
    uint32_t end_level   = start_level + level_count;
    while (start_level < end_level)
    {
        uint32_t same_count = 0;    // layout 相同的连续 level 数量
        for (uint32_t level = start_level; level < end_level; ++level)
            if (_layouts[level] == _layouts[start_level])
                ++same_count;

        transfer_layout_im(_layouts[start_level], new_layout, start_level, same_count);
        start_level += same_count;
    }
}


void Hiss::Image2D::execution_barrier(const Hiss::StageAccess& src, const Hiss::StageAccess& dst,
                                      vk::CommandBuffer command_buffer, uint32_t base_level, uint32_t level_count)
{
    assert(base_level + level_count <= mip_levels._value);

    command_buffer.pipelineBarrier(src.stage, dst.stage, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .srcAccessMask    = {},
                                           .dstAccessMask    = dst.access,
                                           .oldLayout        = _layouts[base_level],
                                           .newLayout        = _layouts[base_level],
                                           .image            = vkimage._value,
                                           .subresourceRange = view(base_level, level_count).range,
                                   }});
}


void Hiss::Image2D::transfer_layout(std::optional<StageAccess> src, std::optional<StageAccess> dst,
                                    vk::CommandBuffer command_buffer, vk::ImageLayout new_layout, bool clear,
                                    uint32_t base_level, uint32_t level_count)
{
    assert(base_level + level_count <= mip_levels._value);

    auto src_stage  = src.has_value() ? src->stage : vk::PipelineStageFlagBits::eTopOfPipe;
    auto dst_stage  = dst.has_value() ? dst->stage : vk::PipelineStageFlagBits::eBottomOfPipe;
    auto src_access = src.has_value() ? src->access : vk::AccessFlags();
    auto dst_access = dst.has_value() ? dst->access : vk::AccessFlags();

    command_buffer.pipelineBarrier(src_stage, dst_stage, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .srcAccessMask = src_access,
                                           .dstAccessMask = dst_access,
                                           .oldLayout     = clear ? vk::ImageLayout::eUndefined : _layouts[base_level],
                                           .newLayout     = new_layout,
                                           .image         = vkimage._value,
                                           .subresourceRange = view(base_level, level_count).range,
                                   }});

    // 更新 layout 数据
    for (int i = 0; i < level_count; ++i)
        _layouts[base_level + i] = new_layout;
}


Hiss::Image2D::View Hiss::Image2D::create_view(uint32_t base_mip, uint32_t mip_count)
{
    vk::ImageSubresourceRange range = {
            .aspectMask     = aspect._value,
            .baseMipLevel   = base_mip,
            .levelCount     = mip_count,
            .baseArrayLayer = 0,
            .layerCount     = 1,
    };

    vk::ImageView view = _device.vkdevice().createImageView(vk::ImageViewCreateInfo{
            .image            = vkimage._value,
            .viewType         = vk::ImageViewType::e2D,
            .format           = format._value,
            .subresourceRange = range,
    });

    if (!name._value.empty())
        _device.set_debug_name(vk::ObjectType::eImageView, (VkImageView) view,
                               fmt::format("{}_view_{}_{}", name._value, base_mip, mip_count));

    return {.vkview = view, .range = range};
}


void Hiss::Image2D::transfer_layout_im(vk::ImageLayout old_layout, vk::ImageLayout new_layout, uint32_t base_mip,
                                       uint32_t mip_count)
{
    vk::ImageMemoryBarrier barrier = {
            .oldLayout        = old_layout,
            .newLayout        = new_layout,
            .image            = vkimage._value,
            .subresourceRange = {.aspectMask     = aspect._value,
                                 .baseMipLevel   = base_mip,
                                 .levelCount     = mip_count,
                                 .baseArrayLayer = 0,
                                 .layerCount     = 1},
    };

    OneTimeCommand command_buffer{_device, _device.command_pool()};
    command_buffer().pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe,
                                     {}, {}, {}, {barrier});
    command_buffer.exec();


    // 将 layout 的记录更新
    for (int i = 0; i < mip_count; ++i)
        _layouts[base_mip + i] = new_layout;
}

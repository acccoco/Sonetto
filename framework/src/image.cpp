#include "vk_common.hpp"
#include "image.hpp"


Hiss::ImageView::ImageView(const Image& image, vk::ImageAspectFlags image_aspect, uint32_t base_mip_level,
                           uint32_t n_mip_level)
    : _device(image.device_get()),
      _format(image.format_get())
{
    _subresource = vk::ImageSubresourceRange{
            .aspectMask     = image_aspect,
            .baseMipLevel   = base_mip_level,
            .levelCount     = n_mip_level,
            .baseArrayLayer = 0,
            .layerCount     = 1,
    };

    _image_view = _device.vkdevice().createImageView(vk::ImageViewCreateInfo{
            .image            = image.vkimage(),
            .viewType         = vk::ImageViewType::e2D,
            .format           = _format,
            .subresourceRange = _subresource,
    });
}


Hiss::Image::~Image()
{
    _device.vkdevice().destroy(_image);
    _device.vkdevice().free(_memory);
}


Hiss::ImageView::~ImageView()
{
    _device.vkdevice().destroy(_image_view);
}


Hiss::Image::Image(Device& device, vk::Format format, const vk::Extent2D& extent, uint32_t mip_levels,
                   const std::vector<uint32_t>& sharing_queues, vk::ImageUsageFlags usage,
                   vk::MemoryPropertyFlags memory_properties, vk::ImageTiling tiling, vk::SampleCountFlagBits samples)
    : _device(device),
      _extent(extent),
      _format(format),
      _mip_levels(mip_levels),
      _tiling(tiling),
      _usage(usage),
      _samples(samples)
{
    assert(mip_levels > 0);

    vk::ImageCreateInfo image_info = {
            .imageType   = vk::ImageType::e2D,
            .format      = _format,
            .extent      = vk::Extent3D{.width = extent.width, .height = extent.height, .depth = 1},
            .mipLevels   = mip_levels,
            .arrayLayers = 1,
            .samples     = samples,
            .tiling      = _tiling,
            .usage       = usage,
            .sharingMode = sharing_queues.empty() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
            .queueFamilyIndexCount = static_cast<uint32_t>(sharing_queues.size()),
            .pQueueFamilyIndices   = sharing_queues.data(),
            .initialLayout         = vk::ImageLayout::eUndefined,
    };

    _image = device.vkdevice().createImage(image_info);

    auto memory_require = device.vkdevice().getImageMemoryRequirements(_image);
    _memory             = device.memory_allocate(memory_require, memory_properties);

    device.vkdevice().bindImageMemory(_image, _memory, 0);
}


Hiss::Image::Image(Hiss::Image::ImageCreateInfo&& create_info)
    : Image(create_info.device, create_info.format, create_info.extent, create_info.mip_levels,
            create_info.sharing_queues, create_info.usage, create_info.memory_properties, create_info.tiling,
            create_info.samples)
{}


void Hiss::Image::layout_tran(vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::ImageAspectFlags image_aspect,
                              uint32_t base_mip_level, uint32_t mip_level_count)
{
    vk::ImageMemoryBarrier barrier = {
            .oldLayout        = old_layout,
            .newLayout        = new_layout,
            .image            = _image,
            .subresourceRange = {.aspectMask     = image_aspect,
                                 .baseMipLevel   = base_mip_level,
                                 .levelCount     = mip_level_count,
                                 .baseArrayLayer = 0,
                                 .layerCount     = 1},
    };

    OneTimeCommand command_buffer{_device, _device.command_pool_graphics()};
    command_buffer().pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe,
                                     {}, {}, {}, {barrier});
    command_buffer.exec();
}


void Hiss::Image::copy_buffer_to_image(vk::Buffer buffer, vk::ImageAspectFlags aspect)
{
    vk::BufferImageCopy copy_info = {
            .bufferOffset = 0,
            /* zero for tightly packed */
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,

            .imageSubresource = vk::ImageSubresourceLayers{.aspectMask     = aspect,
                                                           .mipLevel       = 0,
                                                           .baseArrayLayer = 0,
                                                           .layerCount     = 1},
            .imageOffset      = {0, 0, 0},
            .imageExtent      = {_extent.width, _extent.height, 1},
    };


    Hiss::OneTimeCommand command(_device, _device.command_pool_graphics());
    command().copyBufferToImage(buffer, _image, vk::ImageLayout::eTransferDstOptimal, {copy_info});
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
bool Hiss::Image::mipmap_generate(vk::ImageAspectFlags aspect)
{
    assert(_mip_levels > 0);
    if (_mip_levels == 1)
        return true;

    if (!_device.gpu_get().format_support_linear_filter(_format))
        return false;


    Hiss::OneTimeCommand   command(_device, _device.command_pool_graphics());
    vk::ImageMemoryBarrier barrier = {
            .image            = _image,
            .subresourceRange = {.aspectMask = aspect, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};


    for (struct {
             int32_t  mip_width;
             int32_t  mip_height;
             uint32_t level;
         } _{(int32_t) _extent.width, (int32_t) _extent.height, 0};
         _.level < _mip_levels - 1; ++_.level)
    {
        /* layout transition(level#i): transfer_dst -> transfer_src */
        barrier.subresourceRange.baseMipLevel = _.level;
        barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferRead;
        command().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {},
                                  {}, {barrier});


        /**
         * blit: level#i -> level#i+1
         * dst 的 layout 只允许 3 种，这里 transfer_dst 最好
         */
        auto src_offset = std::array<vk::Offset3D, 2>{
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{_.mip_width, _.mip_height, 1},
        };
        auto dst_offset = std::array<vk::Offset3D, 2>{
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{std::max(1, _.mip_width / 2), std::max(1, _.mip_height / 2), 1},
        };
        vk::ImageBlit blit = {
                .srcSubresource = {.aspectMask = aspect, .mipLevel = _.level, .baseArrayLayer = 0, .layerCount = 1},
                .srcOffsets     = src_offset,
                .dstSubresource = {.aspectMask = aspect, .mipLevel = _.level + 1, .baseArrayLayer = 0, .layerCount = 1},
                .dstOffsets     = dst_offset,
        };
        command().blitImage(_image, vk::ImageLayout::eTransferSrcOptimal, _image, vk::ImageLayout::eTransferDstOptimal,
                            {blit}, vk::Filter::eLinear);


        /* layout transition(level#i): transfer_src -> shader_readonly */
        barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = {};    // 这这是个 execution barrier
        command().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {},
                                  {}, {}, {barrier});


        _.mip_width  = _.mip_width > 1 ? _.mip_width / 2 : _.mip_width;
        _.mip_height = _.mip_height > 1 ? _.mip_height / 2 : _.mip_height;
    }


    /* layout transition(level#n): transfer_dst -> shader_readonly */
    barrier.subresourceRange.baseMipLevel = _mip_levels - 1;
    barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout                     = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask                 = {};    // execution barrier
    command().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {},
                              {}, {barrier});


    command.exec();
    return true;
}
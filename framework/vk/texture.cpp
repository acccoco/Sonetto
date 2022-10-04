#include "vk/texture.hpp"

#include <utility>
#include "utils/tools.hpp"


Hiss::Texture::Texture(Device& device, VmaAllocator allocator, std::string tex_path, bool mipmap)
    : path(std::move(tex_path)),
      _device(device),
      _allocator(allocator)
{
    image_create(mipmap);
    sampler_create();
}


void Hiss::Texture::image_create(bool mipmap)
{
    // 从 image 文件中读取数据
    Hiss::Stbi_8Bit_RAII tex_data(path._value, STBI_rgb_alpha);
    channels = tex_data.channels_in_file();


    // 将数据写入 stage buffer 中
    vk::DeviceSize    image_size = tex_data.width() * tex_data.height() * 4;
    Hiss::StageBuffer stage_buffer(_allocator, image_size);
    stage_buffer.mem_copy(tex_data.data, static_cast<size_t>(image_size));


    /* 计算 mipmap 的级别 */
    // _mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(_width, _height)))) + 1;


    // 创建空的 iamge
    _image = new Image2D(_allocator, _device,
                         Image2D::Info{
                                 .format     = vk::Format::eR8G8B8A8Srgb,
                                 .extent     = {(uint32_t) tex_data.width(), (uint32_t) tex_data.height()},
                                 .mip_levels = 1,
                                 .usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
                                        | vk::ImageUsageFlagBits::eSampled,
                                 .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                 .aspect       = vk::ImageAspectFlagBits::eColor,
                                 .init_layout  = vk::ImageLayout::eTransferDstOptimal,
                         });


    // 将 stagebuffer 的数据写入
    _image->copy_buffer_to_image(stage_buffer.vkbuffer());


    // TODO 添加 mipmap 的支持
    /* generate mipmap */


    // 最后将 layout 转变为适合 shader 读取的形式
    _image->transfer_layout_im(vk::ImageLayout::eShaderReadOnlyOptimal);
}


void Hiss::Texture::sampler_create()
{
    vk::SamplerCreateInfo sampler_info = {
            .magFilter  = vk::Filter::eLinear,
            .minFilter  = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,

            .addressModeU = vk::SamplerAddressMode::eMirroredRepeat,
            .addressModeV = vk::SamplerAddressMode::eMirroredRepeat,
            .addressModeW = vk::SamplerAddressMode::eMirroredRepeat,

            .mipLodBias = 0.f,

            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy    = _device.gpu().properties().limits.maxSamplerAnisotropy,

            // 在 PCF shadow map 中会用到
            .compareEnable = VK_FALSE,
            .compareOp     = vk::CompareOp::eAlways,

            // TODO mipmap 的支持
            .minLod = 0.f,
            .maxLod = static_cast<float>(1),

            .borderColor             = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = VK_FALSE,
    };
    _sampler = _device.vkdevice().createSampler(sampler_info);
}


Hiss::Texture::~Texture()
{
    _device.vkdevice().destroy(_sampler);
    DELETE(_image);
}
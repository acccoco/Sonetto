#include "texture.hpp"

#include <utility>
#include "tools.hpp"


Hiss::Texture::Texture(Hiss::Device& device, std::string tex_path, bool mipmap)
    : _device(device),
      _tex_path(std::move(tex_path)),
      _mip_levels(mipmap)
{
    image_create(mipmap);
    _image_view = new Hiss::ImageView(*_image, vk::ImageAspectFlagBits::eColor, 0, _mip_levels);
    sampler_create();
}


void Hiss::Texture::image_create(bool mipmap)
{
    /* read data from a file and transfer it into the stage_buffer */
    Hiss::Stbi_8Bit_RAII tex_data(_tex_path, STBI_rgb_alpha);
    _width    = tex_data.width();
    _height   = tex_data.height();
    _channels = tex_data.channels_in_file();

    vk::DeviceSize image_size = _width * _height * 4;
    Hiss::Buffer   stage_buffer(_device, image_size, vk::BufferUsageFlagBits::eTransferSrc,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    stage_buffer.memory_copy_in(tex_data.data(), static_cast<size_t>(image_size));


    /* 计算 mipmap 的级别 */
    if (!mipmap)
        _mip_levels = 1;
    else
        _mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(_width, _height)))) + 1;


    /* create a image and copy buffer to the image */
    _image = new Hiss::Image(Hiss::Image::ImageCreateInfo{
            .device     = _device,
            .format     = vk::Format::eR8G8B8A8Srgb,
            .extent     = {_width, _height},
            .mip_levels = _mip_levels,
            /* 要建立 mipmap，因此需要 transfer src */
            .usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
                   | vk::ImageUsageFlagBits::eSampled,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
    });
    _image->layout_tran(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageAspectFlagBits::eColor, 0, _mip_levels);
    _image->copy_buffer_to_image(stage_buffer.vkbuffer(), vk::ImageAspectFlagBits::eColor);


    /* generate mipmap */
    if (_mip_levels > 1)
    {
        if (!_image->mipmap_generate(vk::ImageAspectFlagBits::eColor))
            _device.logger().warn("the format is not supported to be linear filtered");
    }
    else
    {
        _image->layout_tran(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                            vk::ImageAspectFlagBits::eColor, 0, _mip_levels);
    }
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
            .maxAnisotropy    = _device.gpu_get().properties().limits.maxSamplerAnisotropy,

            // 在 PCF shadow map 中会用到
            .compareEnable = VK_FALSE,
            .compareOp     = vk::CompareOp::eAlways,

            .minLod = 0.f,
            .maxLod = static_cast<float>(_mip_levels),

            .borderColor             = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = VK_FALSE,
    };
    _sampler = _device.vkdevice().createSampler(sampler_info);
}


Hiss::Texture::~Texture()
{
    _device.vkdevice().destroy(_sampler);
    DELETE(_image_view);
    DELETE(_image);
}
#include "texture.hpp"

#include <utility>
#include "utils/tools.hpp"
#include "utils/stbi.hpp"


Hiss::Texture::Texture(Device& device, VmaAllocator allocator, std::string tex_path, vk::Format format)
    : path(std::move(tex_path)),
      _device(device),
      _allocator(allocator)
{
    _create_image(format);
    _create_sampler();
}


void Hiss::Texture::_create_image(vk::Format format)
{
    // 从 image 文件中读取数据
    Hiss::Stbi_8Bit_RAII tex_data(path._value, STBI_rgb_alpha);
    channels = tex_data.channels_in_file();


    // 将数据写入 stage buffer 中
    vk::DeviceSize    image_size = tex_data.width() * tex_data.height() * 4;
    Hiss::StageBuffer stage_buffer(_device, _allocator, image_size, "");
    stage_buffer.mem_copy(tex_data.data, static_cast<size_t>(image_size));


    /* 计算 mipmap 的级别 */
    // _mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(_width, _height)))) + 1;


    // 创建空的 iamge
    _image = new Image2D(_allocator, _device,
                         Hiss::Image2DCreateInfo{
                                 .format = format,
                                 .extent = {(uint32_t) tex_data.width(), (uint32_t) tex_data.height()},
                                 .usage  = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
                                        | vk::ImageUsageFlagBits::eSampled,
                                 .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                 .aspect       = vk::ImageAspectFlagBits::eColor,
                                 .init_layout  = vk::ImageLayout::eTransferDstOptimal,
                         });


    // 将 stagebuffer 的数据写入
    _image->copy_buffer_to_image(stage_buffer.vkbuffer());


    // 最后将 layout 转变为适合 shader 读取的形式
    _image->transfer_layout_im(vk::ImageLayout::eShaderReadOnlyOptimal);
}


void Hiss::Texture::_create_sampler()
{
    vk::SamplerCreateInfo sampler_info = {
            .magFilter  = vk::Filter::eLinear,
            .minFilter  = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,

            // 纹理坐标超出 [0, 1) 后，如何处理
            .addressModeU = vk::SamplerAddressMode::eMirroredRepeat,
            .addressModeV = vk::SamplerAddressMode::eMirroredRepeat,
            .addressModeW = vk::SamplerAddressMode::eMirroredRepeat,

            .mipLodBias = 0.f,

            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy    = _device.gpu().properties().limits.maxSamplerAnisotropy,

            // 在 PCF shadow map 中会用到
            .compareEnable = VK_FALSE,
            .compareOp     = vk::CompareOp::eAlways,

            // 用于 clamp LOD 级别的
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
#pragma once

#include "image.hpp"
#include "utils/tools.hpp"


namespace Hiss
{
class Texture
{

public:
    /**
     * 会将所有 level 都设为 shader read only layout
     * @details 不支持 mipmap
     */
    Texture(Device& device, VmaAllocator allocator, std::string tex_path, vk::Format format);
    ~Texture();


private:
    void _create_image(vk::Format format);
    void _create_sampler();

    // members =======================================================

public:
    Prop<uint32_t, Texture>              channels{0};    // 实际的通道数
    Prop<std::filesystem::path, Texture> path;

    Image2D&    image() const { return *_image; }
    vk::Sampler sampler() const { return _sampler; }


private:
    Device&      _device;
    VmaAllocator _allocator{};

    Image2D*    _image   = nullptr;
    vk::Sampler _sampler = VK_NULL_HANDLE;
};

}    // namespace Hiss
#pragma once

#include "vk/image.hpp"
#include "utils/tools.hpp"


namespace Hiss
{
class Texture
{

public:
    /**
     * 会将所有 level 都设为 readonly layout
     */
    Texture(Device& device, VmaAllocator allocator, std::string tex_path, bool mipmap, vk::Format format);
    ~Texture();


private:
    void image_create(bool mipmap, vk::Format format);
    void sampler_create();

    // members =======================================================

public:
    Prop<uint32_t, Texture>              channels{0};    // 实际的通道数
    Prop<std::filesystem::path, Texture> path;

    [[nodiscard]] Image2D&    image() const { return *_image; }
    [[nodiscard]] vk::Sampler sampler() const { return _sampler; }


private:
    Device&      _device;
    VmaAllocator _allocator{};

    Image2D*    _image   = nullptr;
    vk::Sampler _sampler = VK_NULL_HANDLE;
};

}    // namespace Hiss
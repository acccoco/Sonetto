#pragma once

#include "core/image.hpp"
#include "utils/tools.hpp"


namespace Hiss
{
class Texture
{

public:
    /**
     * 会将所有 level 都设为 shader read only layout
     */
    Texture(Device& device, VmaAllocator allocator, std::string tex_path, vk::Format format, bool mipmap = false);
    ~Texture();


private:
    void create_image(bool mipmap, vk::Format format);
    void create_sampler();

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
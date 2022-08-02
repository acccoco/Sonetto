#pragma once

#include "image.hpp"


namespace Hiss
{
class Texture
{

public:
    /**
     * 会将所有 level 都设为 readonly layout
     */
    Texture(Device& device, std::string  tex_path, bool mipmap);
    ~Texture();

    [[nodiscard]] vk::Sampler   sampler() const { return _sampler; }
    [[nodiscard]] vk::Image     image() const { return _image->vkimage(); }
    [[nodiscard]] vk::ImageView image_view() const { return _image_view->view_get(); }


private:
    void image_create(bool mipmap);
    void sampler_create();

    // members =======================================================

private:
    Device&           _device;
    const std::string _tex_path;
    Image*            _image      = nullptr;
    ImageView*        _image_view = nullptr;
    vk::Sampler       _sampler    = VK_NULL_HANDLE;


    uint32_t _width      = {};
    uint32_t _height     = {};
    uint32_t _channels   = {};    // The actual number of components for the image
    uint32_t _mip_levels = {};
};

}    // namespace Hiss
#pragma once
#include "vk_common.hpp"
#include "device.hpp"


namespace Hiss
{

/**
 * 具有 3 个 attachment 的 framebuffer，顺序为：color，depth，resolve
 */
class Framebuffer3
{
public:
    Framebuffer3(Device& device, vk::RenderPass render_pass, const vk::Extent2D& extent, vk::ImageView color_view,
                 vk::ImageView depth_view, vk::ImageView resolve_view);
    ~Framebuffer3();

    [[nodiscard]] vk::Framebuffer framebuffer() const { return _framebuffer; }


private:
    Device&         _device;
    vk::Framebuffer _framebuffer = VK_NULL_HANDLE;
};
}    // namespace Hiss

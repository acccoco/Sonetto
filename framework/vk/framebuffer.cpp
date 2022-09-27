#include "vk/framebuffer.hpp"


Hiss::Framebuffer3::Framebuffer3(Device& device, vk::RenderPass render_pass, const vk::Extent2D& extent,
                                 vk::ImageView color_view, vk::ImageView depth_view, vk::ImageView resolve_view)
    : _device(device)
{
    std::array<vk::ImageView, 3> attachments = {
            color_view,
            depth_view,
            resolve_view,
    };


    vk::FramebufferCreateInfo framebuffer_info = {
            .renderPass      = render_pass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments    = attachments.data(),
            .width           = extent.width,
            .height          = extent.height,
            .layers          = 1,
    };
    _framebuffer = _device.vkdevice().createFramebuffer(framebuffer_info);
}


Hiss::Framebuffer3::~Framebuffer3()
{
    _device.vkdevice().destroy(_framebuffer);
}
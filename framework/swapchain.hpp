#pragma once
#include "image.hpp"
#include "device.hpp"


namespace Hiss
{
enum class Recreate
{
    NEED,
    NO_NEED,
};


class Swapchain
{

public:
    Swapchain(Device& device, Window& window, vk::SurfaceKHR surface);
    ~Swapchain();

    [[nodiscard]] vk::Format                       color_format() const { return _present_format.format; }
    [[nodiscard]] vk::Extent2D                     extent_get() const { return _present_extent; }
    [[nodiscard]] size_t                           images_count() const { return _images.size(); }
    [[nodiscard]] vk::ImageView                    image_view_get(uint32_t index) const;
    [[nodiscard]] vk::Image                        vkimage(uint32_t index) const;
    [[nodiscard]] const vk::ImageSubresourceRange& subresource_range() const { return _subresource_range; }

    void resize();

    std::pair<Hiss::Recreate, uint32_t> image_acquire(vk::Semaphore to_signal);
    [[nodiscard]] Hiss::Recreate        image_submit(uint32_t image_index, vk::Semaphore render_semaphore,
                                                     vk::Semaphore trans_semaphore, vk::Fence transfer_fence,
                                                     vk::CommandBuffer present_command_buffer) const;


private:
    void present_format_choose();
    void present_mode_choose();
    void surface_extent_choose();
    void swapchain_create();
    void image_views_create();


    // member ========================================================


private:
    const Device&              _device;
    const Window&              _window;
    vk::SurfaceKHR             _surface;
    vk::SwapchainKHR           _swapchain;
    std::vector<vk::Image>     _images;
    std::vector<vk::ImageView> _image_views;
    vk::SurfaceFormatKHR       _present_format;
    vk::PresentModeKHR         _present_mode{};
    vk::Extent2D               _present_extent;    // surface 的 extent，以像素为单位

    const vk::ImageSubresourceRange _subresource_range = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
    };
};

}    // namespace Hiss
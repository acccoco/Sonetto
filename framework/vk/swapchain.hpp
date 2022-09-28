#pragma once
#include "image.hpp"
#include "vk/device.hpp"


namespace Hiss
{
enum class EnumRecreate
{
    NEED,
    NO_NEED,
};


class Swapchain
{

public:
    Swapchain(Device& device, Window& window, vk::SurfaceKHR surface);
    ~Swapchain();

    static Swapchain* resize(Swapchain* old, Device& device, Window& window, vk::SurfaceKHR surface)
    {
        DELETE(old);
        return new Swapchain(device, window, surface);
    }

    // 得到 swapchain 中 image 支持的 color format
    [[nodiscard]] vk::Format get_color_format() const;


    // swapchain 中 image 的数量
    [[nodiscard]] size_t get_image_number() const;

    // swapchain 中某个 image 对应的 image view
    [[nodiscard]] vk::ImageView get_image_view(uint32_t index) const;

    // 直接获得某个 image
    [[nodiscard]] vk::Image get_image(uint32_t index) const;


    /**
     * 从 swapchain 中获取 image，用于渲染下一帧
     * 只有 submit 过的 image 才能够被再度 acquire
     * @param to_signal image 可用后，回通过该 semaphore 通知
     * @return 检测 window 大小是否发生变化，决定是否需要 recreate swapchain
     */
    [[nodiscard]] std::pair<Hiss::EnumRecreate, uint32_t> acquire_image(vk::Semaphore to_signal) const;


    /**
     * 告知 present queue，swapchain 上的指定 image 可以 present 了
     * @param transfer_fence 状态为 signaled
     * @return 检测 window 大小是否发生变化，决定是否要 recreate swapchain
     */
    [[nodiscard]] Hiss::EnumRecreate submit_image(uint32_t image_index, vk::Semaphore render_semaphore,
                                                  vk::Semaphore trans_semaphore, vk::Fence transfer_fence,
                                                  vk::CommandBuffer present_command_buffer) const;

    /**
     * 返回 render 过的 image，让 swapchain 显示
     * @param wait_semaphore image 渲染完成后，通过这个 semaphore 通知
     */
    [[nodiscard]] Hiss::EnumRecreate submit_image(uint32_t image_index, vk::Semaphore wait_semaphore) const;


private:
    void choose_present_format();
    void choose_present_mode();
    void choose_surface_extent();
    void create_swapchain();
    void create_image_views();


    // member ========================================================


public:
    // 得到 swapchain 的 extent，单位是 pixel
    Prop<vk::Extent2D, Swapchain> present_extent = {};

private:
    Device& _device;
    Window& _window;

    vk::SurfaceKHR             _surface        = VK_NULL_HANDLE;
    vk::SwapchainKHR           _swapchain      = VK_NULL_HANDLE;
    std::vector<vk::Image>     _images         = {};
    std::vector<vk::ImageView> _image_views    = {};
    vk::SurfaceFormatKHR       _present_format = {};
    vk::PresentModeKHR         _present_mode   = {};
    vk::Extent2D               _present_extent = {};    // surface 的 extent，以像素为单位

    const vk::ImageSubresourceRange _subresource_range = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
    };
};

}    // namespace Hiss
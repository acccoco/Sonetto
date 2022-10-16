#pragma once
#include "image.hpp"
#include "device.hpp"


namespace Hiss
{

class Swapchain
{

public:
    Swapchain(Device& device, Window& window, vk::SurfaceKHR surface);
    ~Swapchain();

    // window 尺寸发生变换，swapchain 的应对
    static Swapchain* resize(Swapchain* old, Device& device, Window& window, vk::SurfaceKHR surface)
    {
        DELETE(old);
        return new Swapchain(device, window, surface);
    }


    /**
     * 从 swapchain 中获取 image，用于渲染下一帧
     * @details 初始情况下，或者将 image 提交给了 swapchian 后，这个 image 的拥有者是 swapchian。
     *  向 swapchain 请求 image 用于渲染时，swapchian 只会返回自己拥有的 image
     * @param to_signal_semaphore image 可用后，回通过该 semaphore 通知
     */
    uint32_t acquire_image(vk::Semaphore to_signal_semaphore, vk::Fence to_signal_fence) const;


    /**
     * 返回 render 过的 image，让 swapchain 显示
     * @details image 在提交之前，拥有者是 application，提交之后，拥有者就变成了 swapchain
     * @param wait_semaphore image 渲染完成后，通过这个 semaphore 通知
     */
    void submit_image(uint32_t image_index, vk::Semaphore wait_semaphore) const;


private:
    vk::SurfaceFormatKHR _choose_present_format();
    vk::PresentModeKHR   _choose_present_mode();
    vk::Extent2D         _choose_surface_extent();
    void                 create_swapchain();


public:
    // 各种属性 =============================================================

    vk::Format color_format() const { return _present_format.format; }
    size_t     image_number() const { return _images2.size(); }

    Hiss::Image2D* get_image(uint32_t index) const { return _images2[index]; }

    // 得到 swapchain 的 extent，单位是 pixel
    Prop<vk::Extent2D, Swapchain> present_extent = {};


private:
    Device& _device;
    Window& _window;

    vk::SurfaceKHR        _surface        = VK_NULL_HANDLE;
    vk::SwapchainKHR      _swapchain      = VK_NULL_HANDLE;
    std::vector<Image2D*> _images2        = {};
    vk::SurfaceFormatKHR  _present_format = {};
    vk::PresentModeKHR    _present_mode   = {};
};

}    // namespace Hiss
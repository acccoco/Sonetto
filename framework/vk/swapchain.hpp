#pragma once
#include "image.hpp"
#include "vk/device.hpp"


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
     * 只有 submit 过的 image 才能够被再度 acquire
     * @param signal_semaphore image 可用后，回通过该 semaphore 通知
     */
    [[nodiscard]] uint32_t acquire_image(vk::Semaphore signal_semaphore) const;


    /**
     * 返回 render 过的 image，让 swapchain 显示
     * @param wait_semaphore image 渲染完成后，通过这个 semaphore 通知
     */
    void submit_image(uint32_t image_index, vk::Semaphore wait_semaphore) const;


private:
    void choose_present_format();
    void choose_present_mode();
    void choose_surface_extent();
    void create_swapchain();


public:
    // 各种属性 =============================================================

    [[nodiscard]] vk::Format color_format() const { return _present_format.format; }
    [[nodiscard]] size_t     image_number() const { return _images2.size(); }

    [[nodiscard]] Hiss::Image2D* get_image(uint32_t index) const { return _images2[index]; }

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
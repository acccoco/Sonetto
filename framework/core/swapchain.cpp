#include "swapchain.hpp"

Hiss::Swapchain::Swapchain(Device& device, Window& window, vk::SurfaceKHR surface)
    : _device(device),
      _window(window),
      _surface(surface)
{
    _present_format = _choose_present_format();
    _present_mode   = _choose_present_mode();
    present_extent  = _choose_surface_extent();

    spdlog::info("[swapchain] image format: {}", vk::to_string(_present_format.format));
    spdlog::info("[swapchain] colorspace: {}", vk::to_string(_present_format.colorSpace));
    spdlog::info("[swapchain] present mode: {}", to_string(_present_mode));
    spdlog::info("[swapchain] present extent: ({}, {})", present_extent._value.width, present_extent._value.height);

    create_swapchain();
    auto images = device.vkdevice().getSwapchainImagesKHR(_swapchain);

    // 在 swapchain 的 vkImage 的基础上创建应用自己的 image 对象
    _images2.resize(images.size());
    for (int i = 0; i < images.size(); ++i)
        _images2[i] =
                new Image2D(device, images[i], fmt::format("swapchain image {}", i), vk::ImageAspectFlagBits::eColor,
                            vk::ImageLayout::eUndefined, color_format(), present_extent._value);

    spdlog::info("[swapchain] image number: {}", _images2.size());
}


Hiss::Swapchain::~Swapchain()
{
    _device.vkdevice().destroy(_swapchain);
    for (auto image: _images2)
        delete image;
}


vk::SurfaceFormatKHR Hiss::Swapchain::_choose_present_format()
{
    std::vector<vk::SurfaceFormatKHR> format_list = _device.gpu().vkgpu().getSurfaceFormatsKHR(_surface);
    if (format_list.empty())
        throw std::runtime_error("fail to find surface format.");


    // 优选 srgb
    for (const auto& format: format_list)
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return format;
    return format_list.front();
}


vk::PresentModeKHR Hiss::Swapchain::_choose_present_mode()
{
    /**
     * 优先选择 mailbox（不会画面撕裂，且延迟相对较低）；候补选择 fifo（不会造成画面撕裂）
     */

    std::vector<vk::PresentModeKHR> present_mode_list = _device.gpu().vkgpu().getSurfacePresentModesKHR(_surface);


    for (const auto& present_mode: present_mode_list)
        /**
         * Mailbox: presentation engine 会等待 vertical blanking period，在此期间才会将 image 更新到 surface 上
         * @details 会有个队列用于处理 present 请求，队列长度为 1。新的请求到来时，如果队列是满的，那么新的请求
         *  会将之前的请求挤出队列，在此之前的 image 都可以被复用了。
         */
        if (present_mode == vk::PresentModeKHR::eMailbox)
        {
            return present_mode;
        }


    /**
     * Fifo: presentation engine 会等待 vertical blanking period，只有在此期间才会将 image 更新到 surface 上
     * @details fifo 是一定会被支持的 prsent mode，不会发生画面撕裂
     * @details 内部会有一个队列，用于接受 present 的请求。在 vertical blanking period 期间，会从队列头部取出
     *  present 请求进行处理；新来的 present 请求追加到队列尾部
     */
    return vk::PresentModeKHR::eFifo;
}


vk::Extent2D Hiss::Swapchain::_choose_surface_extent()
{
    auto capability = _device.gpu().vkgpu().getSurfaceCapabilitiesKHR(_surface);

    /* 询问 glfw，当前窗口的大小是多少（pixel） */
    auto window_extent = _window.get_extent();

    return vk::Extent2D{
            .width  = std::clamp(window_extent.width, capability.minImageExtent.width, capability.maxImageExtent.width),
            .height = std::clamp(window_extent.height, capability.minImageExtent.height,
                                 capability.maxImageExtent.height),
    };
}


void Hiss::Swapchain::create_swapchain()
{
    auto     capability = _device.gpu().vkgpu().getSurfaceCapabilitiesKHR(_surface);
    uint32_t image_cnt  = capability.minImageCount + 1;

    /**
     * 确保 swapchain 中 image 的数量不会超过 surface 的限制
     * \n maxImageCount 为 0 表示没有最大数量的限制
     */
    if (capability.maxImageCount > 0 && image_cnt > capability.maxImageCount)
        image_cnt = capability.maxImageCount;


    _swapchain = _device.vkdevice().createSwapchainKHR(vk::SwapchainCreateInfoKHR{
            .surface          = _surface,
            .minImageCount    = image_cnt,
            .imageFormat      = _present_format.format,
            .imageColorSpace  = _present_format.colorSpace,
            .imageExtent      = present_extent(),
            .imageArrayLayers = 1,
            .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,               // 手动改变 queue owner 即可
            .preTransform     = capability.currentTransform,               // 手机才需要
            .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,    // 多个 surface 的情形
            .presentMode      = _present_mode,
            .clipped          = VK_TRUE,
            .oldSwapchain     = {},
    });
}


uint32_t Hiss::Swapchain::acquire_image(vk::Semaphore to_signal_semaphore, vk::Fence to_signal_fence) const
{
    uint32_t image_idx;
    auto     result = static_cast<vk::Result>(vkAcquireNextImageKHR(_device.vkdevice(), _swapchain, UINT64_MAX,
                                                                    to_signal_semaphore, to_signal_fence, &image_idx));
    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        spdlog::warn("swapchain image out of data (acquire image)");
        return image_idx;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        spdlog::error(to_string(result));
        throw std::runtime_error("failed to acquire swapchain image.");
    }
    return image_idx;
}


void Hiss::Swapchain::submit_image(uint32_t image_index, vk::Semaphore wait_semaphore) const
{
    vk::PresentInfoKHR present_info = {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &wait_semaphore,
            .swapchainCount     = 1,
            .pSwapchains        = &this->_swapchain,
            .pImageIndices      = &image_index,
    };

    vk::Result result = _device.queue().vkqueue().presentKHR(present_info);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        spdlog::warn("swapchain image out of data or suboptimal (present image)");
        return;
    }
    if (result != vk::Result::eSuccess)
    {
        spdlog::error(to_string(result));
        throw std::runtime_error("failed to present swapchain image.");
    }
}
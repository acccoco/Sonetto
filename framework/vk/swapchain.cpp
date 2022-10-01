#include "vk/swapchain.hpp"

Hiss::Swapchain::Swapchain(Device& device, Window& window, vk::SurfaceKHR surface)
    : _device(device),
      _window(window),
      _surface(surface)
{
    _present_format = choose_present_format();
    _present_mode   = choose_present_mode();
    present_extent  = choose_surface_extent();

    spdlog::info("[swapchain] image format: {}", vk::to_string(_present_format.format));
    spdlog::info("[swapchain] colorspace: {}", vk::to_string(_present_format.colorSpace));
    spdlog::info("[swapchain] present mode: {}", to_string(_present_mode));
    spdlog::info("[swapchain] present extent: ({}, {})", present_extent._value.width, present_extent._value.height);

    create_swapchain();
    auto images = device.vkdevice().getSwapchainImagesKHR(_swapchain);

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


vk::SurfaceFormatKHR Hiss::Swapchain::choose_present_format()
{
    std::vector<vk::SurfaceFormatKHR> format_list = _device.gpu().vkgpu().getSurfaceFormatsKHR(_surface);
    if (format_list.empty())
        throw std::runtime_error("fail to find surface format.");

    for (const auto& format: format_list)
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return format;
        }
    return format_list.front();
}


vk::PresentModeKHR Hiss::Swapchain::choose_present_mode()
{
    std::vector<vk::PresentModeKHR> present_mode_list = _device.gpu().vkgpu().getSurfacePresentModesKHR(_surface);
    for (const auto& present_mode: present_mode_list)
        if (present_mode == vk::PresentModeKHR::eMailbox)
        {
            return present_mode;
        }
    return vk::PresentModeKHR::eFifo;
}


vk::Extent2D Hiss::Swapchain::choose_surface_extent()
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
    /**
     * 确定 swapchain 中 image 的数量
     * minImageCount 至少是 1; maxImageCount 为 0 时表示没有限制
     */
    auto     capability = _device.gpu().vkgpu().getSurfaceCapabilitiesKHR(_surface);
    uint32_t image_cnt  = capability.minImageCount + 1;
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


uint32_t Hiss::Swapchain::acquire_image(vk::Semaphore signal_semaphore) const
{
    /* 这里不用 vulkan-cpp，因为错误处理有些问题 */
    uint32_t image_idx;
    auto     result = static_cast<vk::Result>(
            vkAcquireNextImageKHR(_device.vkdevice(), _swapchain, UINT64_MAX, signal_semaphore, {}, &image_idx));
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

    vk::Result result = _device.queue().queue.presentKHR(present_info);

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
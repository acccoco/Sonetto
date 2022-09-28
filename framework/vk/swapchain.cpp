#include "vk/swapchain.hpp"

Hiss::Swapchain::Swapchain(Device& device, Window& window, vk::SurfaceKHR surface)
    : _device(device),
      _window(window),
      _surface(surface)
{
    choose_present_format();
    choose_present_mode();
    choose_surface_extent();

    spdlog::info("swapchain image format: {}", vk::to_string(_present_format.format));
    spdlog::info("swapchain colorspace: {}", vk::to_string(_present_format.colorSpace));

    create_swapchain();
    _images = device.vkdevice().getSwapchainImagesKHR(_swapchain);
    create_image_views();
}


Hiss::Swapchain::~Swapchain()
{
    _device.vkdevice().destroy(_swapchain);
    for (auto& image_view: _image_views)
        _device.vkdevice().destroy(image_view);
    _image_views.clear();
}


void Hiss::Swapchain::choose_present_format()
{
    std::vector<vk::SurfaceFormatKHR> format_list = _device.get_gpu().vkgpu.get().getSurfaceFormatsKHR(_surface);
    for (const auto& format: format_list)
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            _present_format = format;
            return;
        }
    _present_format = format_list[0];
}


void Hiss::Swapchain::choose_present_mode()
{
    std::vector<vk::PresentModeKHR> present_mode_list = _device.get_gpu().vkgpu.get().getSurfacePresentModesKHR(_surface);
    for (const auto& present_mode: present_mode_list)
        if (present_mode == vk::PresentModeKHR::eMailbox)
        {
            _present_mode = present_mode;
            return;
        }
    _present_mode = vk::PresentModeKHR::eFifo;
}


void Hiss::Swapchain::choose_surface_extent()
{
    auto capability = _device.get_gpu().vkgpu().getSurfaceCapabilitiesKHR(_surface);


    /* 询问 glfw，当前窗口的大小是多少（pixel） */
    auto window_extent = _window.get_extent();

    present_extent._value = vk::Extent2D{
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
    auto     capability = _device.get_gpu().vkgpu().getSurfaceCapabilitiesKHR(_surface);
    uint32_t image_cnt  = capability.minImageCount + 1;
    if (capability.maxImageCount > 0 && image_cnt > capability.maxImageCount)
        image_cnt = capability.maxImageCount;


    _swapchain = _device.vkdevice().createSwapchainKHR(vk::SwapchainCreateInfoKHR{
            .surface          = _surface,
            .minImageCount    = image_cnt,
            .imageFormat      = _present_format.format,
            .imageColorSpace  = _present_format.colorSpace,
            .imageExtent      = present_extent.get(),
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


void Hiss::Swapchain::create_image_views()
{
    size_t images_cnt = _images.size();
    _image_views.reserve(images_cnt);
    for (size_t i = 0; i < images_cnt; ++i)
    {
        _image_views.push_back(_device.vkdevice().createImageView(vk::ImageViewCreateInfo{
                .image            = _images[i],
                .viewType         = vk::ImageViewType::e2D,
                .format           = _present_format.format,
                .components       = {.r = vk::ComponentSwizzle::eIdentity,
                                     .g = vk::ComponentSwizzle::eIdentity,
                                     .b = vk::ComponentSwizzle::eIdentity,
                                     .a = vk::ComponentSwizzle::eIdentity},
                .subresourceRange = _subresource_range,
        }));
    }
}


std::pair<Hiss::EnumRecreate, uint32_t> Hiss::Swapchain::acquire_image(vk::Semaphore to_signal) const
{
    /* 这里不用 vulkan-cpp，因为错误处理有些问题 */
    uint32_t image_idx;
    auto     result = static_cast<vk::Result>(
            vkAcquireNextImageKHR(_device.vkdevice(), _swapchain, UINT64_MAX, to_signal, {}, &image_idx));
    if (result == vk::Result::eErrorOutOfDateKHR)
        return std::make_pair(Hiss::EnumRecreate::NEED, 0);
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        throw std::runtime_error("failed to acquire swapchain image.");

    return std::make_pair(Hiss::EnumRecreate::NO_NEED, image_idx);

    /**
     * no need to change queue ownership, because we dont need image content
     */
}


Hiss::EnumRecreate Hiss::Swapchain::submit_image(uint32_t image_index, vk::Semaphore render_semaphore,
                                                 vk::Semaphore trans_semaphore, vk::Fence transfer_fence,
                                                 vk::CommandBuffer present_command_buffer) const
{
    /* present */
    vk::PresentInfoKHR present_info = {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &render_semaphore,
            .swapchainCount     = 1,
            .pSwapchains        = &_swapchain,
            .pImageIndices      = &image_index,
    };
    vk::Result result = _device.queue().queue.presentKHR(present_info);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        return Hiss::EnumRecreate::NEED;
    }
    if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to present swapchain image.");
    }
    return Hiss::EnumRecreate::NO_NEED;
}


vk::ImageView Hiss::Swapchain::get_image_view(uint32_t index) const
{
    assert(index < _image_views.size());
    return _image_views[index];
}


vk::Image Hiss::Swapchain::get_image(uint32_t index) const
{
    assert(index < _images.size());
    return _images[index];
}


vk::Format Hiss::Swapchain::get_color_format() const
{
    return _present_format.format;
}


size_t Hiss::Swapchain::get_image_number() const
{
    return _images.size();
}


Hiss::EnumRecreate Hiss::Swapchain::submit_image(uint32_t image_index, vk::Semaphore wait_semaphore) const
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
        return Hiss::EnumRecreate::NEED;
    if (result != vk::Result::eSuccess)
        throw std::runtime_error("failed to present swapchain image.");
    return Hiss::EnumRecreate::NO_NEED;
}
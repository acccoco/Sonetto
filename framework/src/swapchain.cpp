#include "swapchain.hpp"

Hiss::Swapchain::Swapchain(Device& device, Window& window, vk::SurfaceKHR surface)
    : _device(device),
      _window(window),
      _surface(surface)
{
    present_format_choose();
    present_mode_choose();
    surface_extent_choose();

    _device.logger().info("swapchain image format: {}", vk::to_string(_present_format.format));
    _device.logger().info("swapchain colorspace: {}", vk::to_string(_present_format.colorSpace));

    swapchain_create();
    _images = device.vkdevice().getSwapchainImagesKHR(_swapchain);
    image_views_create();
}


Hiss::Swapchain::~Swapchain()
{
    _device.vkdevice().destroy(_swapchain);
    for (auto& image_view: _image_views)
        _device.vkdevice().destroy(image_view);
    _image_views.clear();
}


void Hiss::Swapchain::resize()
{
    _device.vkdevice().destroy(_swapchain);
    for (auto& image_view: _image_views)
        _device.vkdevice().destroy(image_view);
    _image_views.clear();

    surface_extent_choose();
    swapchain_create();
    _images = _device.vkdevice().getSwapchainImagesKHR(_swapchain);
    image_views_create();
}


void Hiss::Swapchain::present_format_choose()
{
    std::vector<vk::SurfaceFormatKHR> format_list = _device.gpu_get().handle_get().getSurfaceFormatsKHR(_surface);
    for (const auto& format: format_list)
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            _present_format = format;
            return;
        }
    _present_format = format_list[0];
}


void Hiss::Swapchain::present_mode_choose()
{
    std::vector<vk::PresentModeKHR> present_mode_list =
            _device.gpu_get().handle_get().getSurfacePresentModesKHR(_surface);
    for (const auto& present_mode: present_mode_list)
        if (present_mode == vk::PresentModeKHR::eMailbox)
        {
            _present_mode = present_mode;
            return;
        }
    _present_mode = vk::PresentModeKHR::eFifo;
}


void Hiss::Swapchain::surface_extent_choose()
{
    auto capability = _device.gpu_get().handle_get().getSurfaceCapabilitiesKHR(_surface);


    /* 询问 glfw，当前窗口的大小是多少（pixel） */
    auto window_extent = _window.extent_get();

    _present_extent = vk::Extent2D{
            .width  = std::clamp(window_extent.width, capability.minImageExtent.width, capability.maxImageExtent.width),
            .height = std::clamp(window_extent.height, capability.minImageExtent.height,
                                 capability.maxImageExtent.height),
    };
}


void Hiss::Swapchain::swapchain_create()
{
    /**
     * 确定 swapchain 中 image 的数量
     * minImageCount 至少是 1; maxImageCount 为 0 时表示没有限制
     */
    auto     capability = _device.gpu_get().handle_get().getSurfaceCapabilitiesKHR(_surface);
    uint32_t image_cnt  = capability.minImageCount + 1;
    if (capability.maxImageCount > 0 && image_cnt > capability.maxImageCount)
        image_cnt = capability.maxImageCount;


    _swapchain = _device.vkdevice().createSwapchainKHR(vk::SwapchainCreateInfoKHR{
            .surface          = _surface,
            .minImageCount    = image_cnt,
            .imageFormat      = _present_format.format,
            .imageColorSpace  = _present_format.colorSpace,
            .imageExtent      = _present_extent,
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


void Hiss::Swapchain::image_views_create()
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


/**
 * 从 swapchain 中获取用于绘制下一帧的 image
 * @return 检测 window 大小是否发生变化，决定是否需要 recreate swapchain
 */
std::pair<Hiss::Recreate, uint32_t> Hiss::Swapchain::image_acquire(vk::Semaphore to_signal)
{
    /* 这里不用 vulkan-cpp，因为错误处理有些问题 */
    uint32_t image_idx;
    auto     result = static_cast<vk::Result>(
            vkAcquireNextImageKHR(_device.vkdevice(), _swapchain, UINT64_MAX, to_signal, {}, &image_idx));
    if (result == vk::Result::eErrorOutOfDateKHR)
        return std::make_pair(Hiss::Recreate::NEED, 0);
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        throw std::runtime_error("failed to acquire swapchain image.");

    return std::make_pair(Hiss::Recreate::NO_NEED, image_idx);

    /**
     * no need to change queue ownership, because we dont need image content
     */
}


/**
 * 告知 present queue，swapchain 上的指定 image 可以 present 了
 * @return 检测 window 大小是否发生变化，决定是否要 recreate swapchain
 */
Hiss::Recreate Hiss::Swapchain::image_submit(uint32_t image_index, vk::Semaphore render_semaphore,
                                             vk::Semaphore trans_semaphore, vk::Fence transfer_fence,
                                             vk::CommandBuffer present_command_buffer) const
{
    /* queue ownership transfer: acquire */
    bool need_ownership_trans = !Queue::is_same_queue_family(_device.present_queue(), _device.graphics_queue());
    if (need_ownership_trans)
    {
        present_command_buffer.reset();
        present_command_buffer.begin(vk::CommandBufferBeginInfo{});
        /**
         * 不需要 src/dst 的 access 和 stage，这些都由前后的 semaphore 保证
         */
        vk::ImageMemoryBarrier acquire_barrier = {
                .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout           = vk::ImageLayout::ePresentSrcKHR,
                .srcQueueFamilyIndex = _device.graphics_queue().family_index,
                .dstQueueFamilyIndex = _device.present_queue().family_index,
                .image               = this->_images[image_index],
                .subresourceRange    = {.aspectMask     = vk::ImageAspectFlagBits::eColor,
                                        .baseMipLevel   = 0,
                                        .levelCount     = 1,
                                        .baseArrayLayer = 0,
                                        .layerCount     = 1},
        };
        present_command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                               vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, {acquire_barrier});
        present_command_buffer.end();

        vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        _device.vkdevice().resetFences({transfer_fence});
        _device.present_queue().queue.submit({vk::SubmitInfo{
                                                     .waitSemaphoreCount   = 1,
                                                     .pWaitSemaphores      = &render_semaphore,
                                                     .pWaitDstStageMask    = &wait_stage,
                                                     .commandBufferCount   = 1,
                                                     .pCommandBuffers      = &present_command_buffer,
                                                     .signalSemaphoreCount = 1,
                                                     .pSignalSemaphores    = &trans_semaphore,
                                             }},
                                             transfer_fence);
    }


    /* present */
    vk::PresentInfoKHR present_info = {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = need_ownership_trans ? &trans_semaphore : &render_semaphore,
            .swapchainCount     = 1,
            .pSwapchains        = &_swapchain,
            .pImageIndices      = &image_index,
    };
    vk::Result result = _device.present_queue().queue.presentKHR(present_info);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        return Hiss::Recreate::NEED;
    }
    if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to present swapchain image.");
    }
    return Hiss::Recreate::NO_NEED;
}


vk::ImageView Hiss::Swapchain::image_view_get(uint32_t index) const
{
    assert(index < _image_views.size());
    return _image_views[index];
}


vk::Image Hiss::Swapchain::vkimage(uint32_t index) const
{
    assert(index < _images.size());
    return _images[index];
}
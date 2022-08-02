#include "hello_triangle.hpp"


APP_RUN(HelloTriangle)


void HelloTriangle::prepare()
{
    Hiss::ExampleBase::prepare();

    _logger->info("[HelloTriangle] prepare");

    pipeline_prepare();
    _vertex_buffer = std::make_shared<Hiss::VertexBuffer<Hiss::Vertex2DColor>>(*_device, vertices);
    _index_buffer  = std::make_shared<Hiss::IndexBuffer>(*_device, indices);
}


void HelloTriangle::pipeline_prepare()
{
    auto vertex_shader_module   = shader_load(shader_vert_path, vk::ShaderStageFlagBits::eVertex);
    auto fragment_shader_module = shader_load(shader_frag_path, vk::ShaderStageFlagBits::eFragment);
    _pipeline_state.shader_stage_add(vertex_shader_module);
    _pipeline_state.shader_stage_add(fragment_shader_module);

    _pipeline_state.vertex_input_binding_set(Hiss::Vertex2DColor::binding_description_get(0));
    _pipeline_state.vertex_input_attribute_set(Hiss::Vertex2DColor::attribute_description_get(0));

    _pipeline_state.viewport_set(_swapchain->extent_get());

    _pipeline_layout = _device->vkdevice().createPipelineLayout({});
    _pipeline_state.pipeline_layout_set(_pipeline_layout);

    _pipeline = _pipeline_state.generate(*_device, _simple_render_pass, 0);
}


void HelloTriangle::command_record(vk::CommandBuffer command_buffer, uint32_t swapchain_image_index)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});


    /**
     * depth barrier: 只是 execution barrier，确保对 depth buffer 的写入不会乱序
     * color barrier: 负责将 swapchain 的 present layout 转换为 color layout
     */
    vk::ImageMemoryBarrier depth_barrier = {
            .srcAccessMask    = {},    // execution barrier
            .dstAccessMask    = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            .oldLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .newLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .image            = _depth_image->vkimage(),
            .subresourceRange = _depth_image_view->subresource_range(),
    };
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eLateFragmentTests,
                                   vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, {}, {}, {depth_barrier});
    vk::ImageMemoryBarrier color_barrier = {
            .dstAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout        = vk::ImageLayout::eUndefined,
            .newLayout        = vk::ImageLayout::eColorAttachmentOptimal,
            .image            = _swapchain->vkimage(swapchain_image_index),
            .subresourceRange = _swapchain->subresource_range(),
    };
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, {color_barrier});


    /* 绘制过程 */
    std::array<vk::ClearValue, 2> clear_values = {
            vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.2f, 0.2f, 0.2f, 1.f}}},
            vk::ClearValue{.depthStencil = {1.f, 0}},
    };
    command_buffer.beginRenderPass(
            vk::RenderPassBeginInfo{
                    .renderPass      = _simple_render_pass,
                    .framebuffer     = _framebuffers[swapchain_image_index],
                    .renderArea      = {.offset = {0, 0}, .extent = _swapchain->extent_get()},
                    .clearValueCount = static_cast<uint32_t>(clear_values.size()),
                    .pClearValues    = clear_values.data(),
            },
            vk::SubpassContents::eInline);
    command_buffer.bindVertexBuffers(0, {_vertex_buffer->vkbuffer()}, {0});
    command_buffer.bindIndexBuffer(_index_buffer->vkbuffer(), 0, vk::IndexType::eUint32);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
    command_buffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    // command_buffer.draw(3, 1, 0, 0);
    command_buffer.endRenderPass();


    /* queue family ownership transfer: release and layout transition */
    vk::ImageMemoryBarrier release_barrier = {
            .srcAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout           = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = _device->graphics_queue().family_index,
            .dstQueueFamilyIndex = _device->present_queue().family_index,
            .image               = _swapchain->vkimage(swapchain_image_index),
            .subresourceRange    = _swapchain->subresource_range(),
    };
    if (!Hiss::Queue::is_same_queue_family(_device->present_queue(), _device->graphics_queue()))
    {
        release_barrier.srcQueueFamilyIndex = _device->graphics_queue().family_index;
        release_barrier.dstQueueFamilyIndex = _device->present_queue().family_index;
    }

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, {release_barrier});


    command_buffer.end();
}


void HelloTriangle::draw()
{
    /* 不要超出 in-flight 的最大帧数 */
    (void) _device->vkdevice().waitForFences(
            {_render_context->current_fence_render(), _render_context->current_fence_transfer()}, VK_TRUE, UINT64_MAX);


    /* acquire image from swapchain */
    Hiss::Recreate need_recreate;
    uint32_t       swapchain_image_index;
    std::tie(need_recreate, swapchain_image_index) =
            _swapchain->image_acquire(_render_context->current_semaphore_acquire());
    if (need_recreate == Hiss::Recreate::NEED)
    {
        resize();
        return;
    }


    /* draw */
    _render_context->current_command_buffer_graphics().reset();
    command_record(_render_context->current_command_buffer_graphics(), swapchain_image_index);
    std::vector<vk::PipelineStageFlags> wait_stages = {vk::PipelineStageFlagBits::eEarlyFragmentTests};
    _device->vkdevice().resetFences({_render_context->current_fence_render()});
    _device->graphics_queue().queue.submit(
            {
                    vk::SubmitInfo{.waitSemaphoreCount   = 1,
                                   .pWaitSemaphores      = &_render_context->current_semaphore_acquire(),
                                   .pWaitDstStageMask    = wait_stages.data(),
                                   .commandBufferCount   = 1,
                                   .pCommandBuffers      = &_render_context->current_command_buffer_graphics(),
                                   .signalSemaphoreCount = 1,
                                   .pSignalSemaphores    = &_render_context->current_semaphore_render()},
            },
            _render_context->current_fence_render());


    /* present */
    need_recreate = _swapchain->image_submit(swapchain_image_index, _render_context->current_semaphore_render(),
                                             _render_context->current_semaphore_transfer(),
                                             _render_context->current_fence_transfer(),
                                             _render_context->current_command_buffer_present());
    if (need_recreate == Hiss::Recreate::NEED)
    {
        resize();
    }


    _render_context->next_frame();
}


void HelloTriangle::update() noexcept
{
    Hiss::ExampleBase::update();
    draw();
}


void HelloTriangle::resize()
{
    ExampleBase::resize();
    _logger->info("[HelloTriangle] resize");

    _device->vkdevice().destroy(_pipeline);

    _pipeline_state.viewport_set(_swapchain->extent_get());
    _pipeline = _pipeline_state.generate(*_device, _simple_render_pass, 0);
}


void HelloTriangle::clean()
{
    _logger->info("[HelloTriangle] clean");

    _vertex_buffer = nullptr;
    _index_buffer  = nullptr;
    _device->vkdevice().destroy(_pipeline);
    _device->vkdevice().destroy(_pipeline_layout);

    ExampleBase::clean();
}
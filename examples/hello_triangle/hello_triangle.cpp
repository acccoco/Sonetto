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
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eLateFragmentTests,
                                   vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .srcAccessMask    = {},    // execution barrier
                                           .dstAccessMask    = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                                           .oldLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                           .newLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                           .image            = _depth_image->vkimage(),
                                           .subresourceRange = _depth_image_view->subresource_range(),
                                   }});

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .dstAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite,
                                           .oldLayout        = vk::ImageLayout::eUndefined,
                                           .newLayout        = vk::ImageLayout::eColorAttachmentOptimal,
                                           .image            = _swapchain->vkimage(swapchain_image_index),
                                           .subresourceRange = _swapchain->subresource_range(),
                                   }});


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
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .srcAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
                                           .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
                                           .newLayout           = vk::ImageLayout::ePresentSrcKHR,
                                           .srcQueueFamilyIndex = _device->queue_graphics().family_index,
                                           .dstQueueFamilyIndex = _device->queue_present().family_index,
                                           .image               = _swapchain->vkimage(swapchain_image_index),
                                           .subresourceRange    = _swapchain->subresource_range(),
                                   }});


    command_buffer.end();
}


void HelloTriangle::update(double delte_time) noexcept
{
    Hiss::ExampleBase::update(delte_time);


    frame_prepare();

    /* draw */
    vk::CommandBuffer command_buffer = current_frame().command_buffer_graphics();
    command_record(command_buffer, current_swapchain_image_index());

    std::array<vk::PipelineStageFlags, 1> wait_stages       = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    std::array<vk::Semaphore, 1>          wait_semaphores   = {current_frame().semaphore_swapchain_acquire()};
    std::array<vk::Semaphore, 1>          signal_semaphores = {current_frame().semaphore_render_complete()};

    vk::Fence fence = _device->fence_pool().get(false);
    _device->queue_graphics().queue.submit(
            {
                    vk::SubmitInfo{.waitSemaphoreCount   = static_cast<uint32_t>(wait_semaphores.size()),
                                   .pWaitSemaphores      = wait_semaphores.data(),
                                   .pWaitDstStageMask    = wait_stages.data(),
                                   .commandBufferCount   = 1,
                                   .pCommandBuffers      = &command_buffer,
                                   .signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size()),
                                   .pSignalSemaphores    = signal_semaphores.data()},
            },
            fence);
    current_frame().fence_push(fence);


    frame_submit();
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
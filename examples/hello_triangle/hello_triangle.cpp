#include "hello_triangle.hpp"


APP_RUN(HelloTriangle)


void HelloTriangle::prepare()
{
    Hiss::VkApplication::prepare();

    _logger->info("[HelloTriangle] prepare");

    // 初始化各种字段
    init_pipeline();
    _vertex_buffer     = std::make_shared<Hiss::VertexBuffer<Hiss::Vertex2DColor>>(*_device, vertices);
    _index_buffer      = std::make_shared<Hiss::IndexBuffer>(*_device, indices);
    depth_image_2      = Hiss::Image::create_depth_attach(*_device, get_extent(), "depth_image");
    depth_image_view_2 = new Hiss::ImageView(*depth_image_2, vk::ImageAspectFlagBits::eDepth, 0, 1);

    // 初始化 per frame payload
    _payloads.reserve(_frame_manager->frames.get().size());
    for (auto frame: _frame_manager->frames.get())
    {
        FramePayload payload;
        payload.command_buffer = _device->command_pool_graphics().command_buffer_create(1).front();
        payload.framebuffer    = _device->create_framebuffer(
                _simple_render_pass, {frame->image_view.get(), depth_image_view_2->view_get()}, get_extent());
        payload.color_attach_info = vk::RenderingAttachmentInfo{
                .imageView   = frame->image_view.get(),
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp      = vk::AttachmentLoadOp::eClear,
                .storeOp     = vk::AttachmentStoreOp::eStore,
                .clearValue  = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.2f, 0.2f, 0.2f, 1.f}}},
        };
        payload.depth_attach_info = vk::RenderingAttachmentInfo{
                .imageView   = _depth_image_view->view_get(),
                .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .loadOp      = vk::AttachmentLoadOp::eClear,
                .storeOp     = vk::AttachmentStoreOp::eDontCare,
                .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
        };
        _payloads.push_back(payload);
    }

    _logger->info("[hello] prepare end");
}


void HelloTriangle::init_pipeline()
{
    auto vertex_shader_module   = _shader_loader->load(shader_vert_path, vk::ShaderStageFlagBits::eVertex);
    auto fragment_shader_module = _shader_loader->load(shader_frag_path, vk::ShaderStageFlagBits::eFragment);
    _pipeline_template.shader_stage_add(vertex_shader_module);
    _pipeline_template.shader_stage_add(fragment_shader_module);

    _pipeline_template.vertex_input_binding_set(Hiss::Vertex2DColor::binding_description_get(0));
    _pipeline_template.vertex_input_attribute_set(Hiss::Vertex2DColor::attribute_description_get(0));

    _pipeline_template.viewport_set(get_extent());

    _pipeline_layout = _device->vkdevice().createPipelineLayout({});
    _pipeline_template.pipeline_layout_set(_pipeline_layout);

    // FIXME
    _pipeline_template.depth_format  = _device->gpu_get().depth_stencil_format.get();
    _pipeline_template.color_formats = {get_color_format()};

    _pipeline = _pipeline_template.generate(*_device, _simple_render_pass, 0);
}


void HelloTriangle::record_command(vk::CommandBuffer command_buffer, const FramePayload& payload,
                                   const Hiss::Frame2& frame)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});


    // depth attachment execution barrier: 确保对 depth buffer 的写入不会乱序
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eLateFragmentTests,
                                   vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .srcAccessMask    = {},    // execution barrier
                                           .dstAccessMask    = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                                           .oldLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                           .newLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                           .image            = depth_image_2->vkimage(),
                                           .subresourceRange = depth_image_view_2->subresource_range(),
                                   }});

    // color attachment layout transfer: undefined -> present （无需保留之前的内容）
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .dstAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite,
                                           .oldLayout        = vk::ImageLayout::eUndefined,
                                           .newLayout        = vk::ImageLayout::eColorAttachmentOptimal,
                                           .image            = frame.image.get(),
                                           .subresourceRange = Hiss::COLOR_SUBRESOURCE_RANGE,
                                   }});


    /* 绘制过程 */
    command_buffer.beginRendering(vk::RenderingInfo{
            .renderArea           = {.offset = {0, 0}, .extent = get_extent()},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &payload.color_attach_info,
            .pDepthAttachment     = &payload.depth_attach_info,
    });


    command_buffer.bindVertexBuffers(0, {_vertex_buffer->vkbuffer()}, {0});
    command_buffer.bindIndexBuffer(_index_buffer->vkbuffer(), 0, vk::IndexType::eUint32);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
    command_buffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    command_buffer.endRendering();


    /* color attachment layout transition: color -> present */
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .srcAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite,
                                           .oldLayout        = vk::ImageLayout::eColorAttachmentOptimal,
                                           .newLayout        = vk::ImageLayout::ePresentSrcKHR,
                                           .image            = frame.image.get(),
                                           .subresourceRange = _swapchain->get_image_subresource_range(),
                                   }});


    command_buffer.end();
}


void HelloTriangle::update(double delte_time) noexcept
{
    Hiss::VkApplication::update(delte_time);

    auto  frame   = _frame_manager->acquire_frame();
    auto& payload = _payloads[frame->frame_id.get()];
    // prepare_frame();

    /* draw */
    vk::CommandBuffer command_buffer = payload.command_buffer;
    record_command(command_buffer, payload, *frame);

    std::array<vk::PipelineStageFlags, 1> wait_stages       = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    std::array<vk::Semaphore, 1>          wait_semaphores   = {frame->acquire_semaphore.get()};
    std::array<vk::Semaphore, 1>          signal_semaphores = {frame->submit_semaphore.get()};

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
            frame->insert_fence());

    _frame_manager->submit_frame(frame);
}


void HelloTriangle::resize()
{
    VkApplication::resize();
    _logger->info("[HelloTriangle] resize");

    // pipeline
    _device->vkdevice().destroy(_pipeline);
    _pipeline_template.viewport_set(_swapchain->get_extent());
    _pipeline = _pipeline_template.generate(*_device, _simple_render_pass, 0);

    // depth attachmetn
    DELETE(depth_image_2);
    DELETE(depth_image_view_2);
    depth_image_2      = Hiss::Image::create_depth_attach(*_device, get_extent(), "depth_image");
    depth_image_view_2 = new Hiss::ImageView(*depth_image_2, vk::ImageAspectFlagBits::eDepth, 0, 1);

    // payload
    for (auto frame: _frame_manager->frames.get())
    {
        // framebuffer
        auto& payload = _payloads[frame->frame_id.get()];
        _device->vkdevice().destroy(payload.framebuffer);
        payload.framebuffer = _device->create_framebuffer(
                _simple_render_pass, {frame->image_view.get(), depth_image_view_2->view_get()}, get_extent());
        payload.depth_attach_info.imageView = depth_image_view_2->view_get();
        payload.color_attach_info.imageView = frame->image_view.get();
    }
}


void HelloTriangle::clean()
{
    _logger->info("[HelloTriangle] clean");

    // 清理 payload
    for (auto& payload: _payloads)
    {
        _device->vkdevice().destroy(payload.framebuffer);
    }

    // 清理其他的
    DELETE(depth_image_2);
    DELETE(depth_image_view_2);
    _vertex_buffer = nullptr;
    _index_buffer  = nullptr;
    _device->vkdevice().destroy(_pipeline);
    _device->vkdevice().destroy(_pipeline_layout);

    VkApplication::clean();
}
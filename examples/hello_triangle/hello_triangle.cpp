#include "hello_triangle.hpp"
#include "run.hpp"
#include "vk/vk_common.hpp"


RUN(HelloTriangle);

void HelloTriangle::prepare()
{
    spdlog::info("[HelloTriangle] prepare");

    // 初始化各种字段
    init_pipeline();
    // _vertex_buffer     = std::make_shared<Hiss::VertexBuffer<Hiss::Vertex2DColor>>(app.device(), vertices);
    // _index_buffer      = std::make_shared<Hiss::IndexBuffer>(app.device(), indices);
    _index_buffer2   = new Hiss::IndexBuffer2(app.device(), app.allocator, indices);
    _vertex_buffer2  = new Hiss::VertexBuffer2<Hiss::Vertex2DColor>(app.device(), app.allocator, vertices);
    depth_image      = Hiss::Image::create_depth_attach(app.device(), app.get_extent(), "depth_image");
    depth_image_view = new Hiss::ImageView(*depth_image, vk::ImageAspectFlagBits::eDepth, 0, 1);

    // 初始化 per frame payload
    _payloads.reserve(app.frame_manager().frames.get().size());
    for (auto frame: app.frame_manager().frames.get())
    {
        FramePayload payload;
        payload.command_buffer    = app.device().command_pool().command_buffer_create(1).front();
        payload.color_attach_info = vk::RenderingAttachmentInfo{
                .imageView   = frame->image_view.get(),
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp      = vk::AttachmentLoadOp::eClear,
                .storeOp     = vk::AttachmentStoreOp::eStore,
                .clearValue  = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.2f, 0.2f, 0.2f, 1.f}}},
        };
        payload.depth_attach_info = vk::RenderingAttachmentInfo{
                .imageView   = depth_image_view->view_get(),
                .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .loadOp      = vk::AttachmentLoadOp::eClear,
                .storeOp     = vk::AttachmentStoreOp::eDontCare,
                .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
        };
        _payloads.push_back(payload);
    }

    spdlog::info("[hello] prepare end");
}


void HelloTriangle::init_pipeline()
{
    auto vertex_shader_module   = app.shader_loader().load(shader_vert_path, vk::ShaderStageFlagBits::eVertex);
    auto fragment_shader_module = app.shader_loader().load(shader_frag_path, vk::ShaderStageFlagBits::eFragment);
    _pipeline_template.shader_stage_add(vertex_shader_module);
    _pipeline_template.shader_stage_add(fragment_shader_module);

    _pipeline_template.vertex_input_binding_set(Hiss::Vertex2DColor::binding_description_get(0));
    _pipeline_template.vertex_input_attribute_set(Hiss::Vertex2DColor::attribute_description_get(0));

    _pipeline_template.viewport_set(app.get_extent());

    _pipeline_layout = app.device().vkdevice().createPipelineLayout({});
    _pipeline_template.pipeline_layout_set(_pipeline_layout);

    // FIXME 这是 dynamic rendering
    _pipeline_template.depth_format  = app.device().get_gpu().depth_stencil_format.get();
    _pipeline_template.color_formats = {app.get_color_format()};

    _pipeline = _pipeline_template.generate(app.device(), VK_NULL_HANDLE, 0);
}


void HelloTriangle::record_command(vk::CommandBuffer command_buffer, const FramePayload& payload,
                                   const Hiss::Frame& frame)
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
                                           .image            = depth_image->vkimage(),
                                           .subresourceRange = depth_image_view->subresource_range(),
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
            .renderArea           = {.offset = {0, 0}, .extent = app.get_extent()},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &payload.color_attach_info,
            .pDepthAttachment     = &payload.depth_attach_info,
    });


    command_buffer.bindVertexBuffers(0, {_vertex_buffer2->buffer()}, {0});
    command_buffer.bindIndexBuffer(_index_buffer2->buffer(), 0, vk::IndexType::eUint32);
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
                                           .subresourceRange = Hiss::COLOR_SUBRESOURCE_RANGE,
                                   }});


    command_buffer.end();
}


void HelloTriangle::update() noexcept
{
    auto& payload = _payloads[app.current_frame().frame_id.get()];
    // prepare_frame();

    /* draw */
    vk::CommandBuffer command_buffer = payload.command_buffer;
    record_command(command_buffer, payload, app.current_frame());

    std::array<vk::PipelineStageFlags, 1> wait_stages       = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    std::array<vk::Semaphore, 1>          wait_semaphores   = {app.current_frame().acquire_semaphore.get()};
    std::array<vk::Semaphore, 1>          signal_semaphores = {app.current_frame().submit_semaphore.get()};

    app.device().queue().queue.submit(
            {
                    vk::SubmitInfo{.waitSemaphoreCount   = static_cast<uint32_t>(wait_semaphores.size()),
                                   .pWaitSemaphores      = wait_semaphores.data(),
                                   .pWaitDstStageMask    = wait_stages.data(),
                                   .commandBufferCount   = 1,
                                   .pCommandBuffers      = &command_buffer,
                                   .signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size()),
                                   .pSignalSemaphores    = signal_semaphores.data()},
            },
            app.current_frame().insert_fence());
}


void HelloTriangle::resize()
{
    // pipeline
    app.device().vkdevice().destroy(_pipeline);
    _pipeline_template.viewport_set(app.get_extent());
    _pipeline = _pipeline_template.generate(app.device(), VK_NULL_HANDLE, 0);

    // depth attachmetn
    DELETE(depth_image);
    DELETE(depth_image_view);
    depth_image      = Hiss::Image::create_depth_attach(app.device(), app.get_extent(), "depth_image");
    depth_image_view = new Hiss::ImageView(*depth_image, vk::ImageAspectFlagBits::eDepth, 0, 1);

    // payload
    for (auto frame: app.frame_manager().frames.get())
    {
        // framebuffer
        auto& payload                       = _payloads[frame->frame_id.get()];
        payload.depth_attach_info.imageView = depth_image_view->view_get();
        payload.color_attach_info.imageView = frame->image_view.get();
    }
}


void HelloTriangle::clean()
{
    spdlog::info("[HelloTriangle] clean");

    // 清理 payload
    for (auto& payload: _payloads)
    {}

    // 清理其他的
    DELETE(depth_image);
    DELETE(depth_image_view);
    delete _vertex_buffer2;
    delete _index_buffer2;
    app.device().vkdevice().destroy(_pipeline);
    app.device().vkdevice().destroy(_pipeline_layout);
}
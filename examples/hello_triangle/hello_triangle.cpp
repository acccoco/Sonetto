#include "hello_triangle.hpp"
#include "run.hpp"
#include "vk/vk_common.hpp"


RUN(HelloTriangle)


void HelloTriangle::prepare()
{
    spdlog::info("[app] prepare");

    // 初始化各种字段
    init_pipeline();

    // 顶点数据
    _index_buffer  = new Hiss::IndexBuffer2(engine.device(), engine.allocator, indices);
    _vertex_buffer = new Hiss::VertexBuffer2<Hiss::Vertex2DColor>(engine.device(), engine.allocator, vertices);

    // 深度缓冲
    _depth_image = engine.create_depth_image();


    // 初始化 per frame payload
    _payloads.reserve(engine.frame_manager().frames().size());
    for (auto frame: engine.frame_manager().frames())
    {
        FramePayload payload;
        payload.command_buffer = engine.device().command_pool().command_buffer_create(1).front();
        _payloads.push_back(payload);
    }
}


void HelloTriangle::init_pipeline()
{
    auto vertex_shader_module   = engine.shader_loader().load(shader_vert_path, vk::ShaderStageFlagBits::eVertex);
    auto fragment_shader_module = engine.shader_loader().load(shader_frag_path, vk::ShaderStageFlagBits::eFragment);
    _pipeline_template.shader_stages.push_back(vertex_shader_module);
    _pipeline_template.shader_stages.push_back(fragment_shader_module);

    _pipeline_template.vertex_bindings   = Hiss::Vertex2DColor::get_binding_description(0);
    _pipeline_template.vertex_attributes = Hiss::Vertex2DColor::get_attribute_description(0);

    _pipeline_template.set_viewport(engine.extent());

    _pipeline_layout                   = engine.vkdevice().createPipelineLayout({});
    _pipeline_template.pipeline_layout = _pipeline_layout;

    _pipeline_template.depth_format  = engine.device().gpu().depth_stencil_format();
    _pipeline_template.color_formats = {engine.color_format()};

    _pipeline = _pipeline_template.generate(engine.device());
}


void HelloTriangle::record_command(vk::CommandBuffer command_buffer, const FramePayload& payload,
                                   const Hiss::Frame& frame)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});


    // depth attachment execution barrier: 确保对 depth buffer 的写入不会乱序
    _depth_image->execution_barrier(
            {vk::PipelineStageFlagBits::eLateFragmentTests},
            {vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::AccessFlagBits::eDepthStencilAttachmentWrite},
            command_buffer);


    // color attachment layout transfer: undefined -> present （无需保留之前的内容）
    frame.image().transfer_layout({},
                                  Hiss::StageAccess{vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::AccessFlagBits::eColorAttachmentWrite},
                                  command_buffer, vk::ImageLayout::eColorAttachmentOptimal, true);

    auto color_attach_info = vk::RenderingAttachmentInfo{
            .imageView   = frame.image().view().vkview,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.2f, 0.2f, 0.2f, 1.f}}},
    };
    auto depth_attach_info = vk::RenderingAttachmentInfo{
            .imageView   = _depth_image->view().vkview,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eDontCare,
            .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
    };

    /* 绘制过程 */
    command_buffer.beginRendering(vk::RenderingInfo{
            .renderArea           = {.offset = {0, 0}, .extent = engine.extent()},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &color_attach_info,
            .pDepthAttachment     = &depth_attach_info,
    });


    command_buffer.bindVertexBuffers(0, {_vertex_buffer->buffer()}, {0});
    command_buffer.bindIndexBuffer(_index_buffer->buffer(), 0, vk::IndexType::eUint32);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
    command_buffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    command_buffer.endRendering();


    /* color attachment layout transition: color -> present */
    frame.image().transfer_layout(Hiss::StageAccess{vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::AccessFlagBits::eColorAttachmentWrite},
                                  {}, command_buffer, vk::ImageLayout::ePresentSrcKHR);


    command_buffer.end();
}


void HelloTriangle::update() noexcept
{
    auto& frame   = engine.current_frame();
    auto& payload = _payloads[frame.frame_id()];

    /* draw */
    record_command(payload.command_buffer, payload, frame);

    vk::PipelineStageFlags wait_stage       = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::Semaphore          wait_semaphore   = frame.acquire_semaphore();
    vk::Semaphore          signal_semaphore = frame.submit_semaphore();

    engine.device().vkqueue().submit({vk::SubmitInfo{
                                             .waitSemaphoreCount   = 1,
                                             .pWaitSemaphores      = &wait_semaphore,
                                             .pWaitDstStageMask    = &wait_stage,
                                             .commandBufferCount   = 1,
                                             .pCommandBuffers      = &payload.command_buffer,
                                             .signalSemaphoreCount = 1,
                                             .pSignalSemaphores    = &signal_semaphore,
                                     }},
                                     frame.insert_fence());
}


void HelloTriangle::resize()
{
    // pipeline
    engine.device().vkdevice().destroy(_pipeline);
    _pipeline_template.set_viewport(engine.extent());
    _pipeline = _pipeline_template.generate(engine.device());

    // depth attachmetn
    delete _depth_image;
    _depth_image = engine.create_depth_image();

    // payload
}


void HelloTriangle::clean()
{
    spdlog::info("[app] clean");

    // 清理 payload
    for (auto& payload: _payloads)
        ;

    // 清理其他的
    delete _depth_image;
    delete _vertex_buffer;
    delete _index_buffer;
    engine.device().vkdevice().destroy(_pipeline);
    engine.device().vkdevice().destroy(_pipeline_layout);
}
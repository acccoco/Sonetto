#include "hello_triangle.hpp"
#include "run.hpp"
#include "core/vk_common.hpp"


RUN(Hello::App)


void Hello::App::prepare()
{
    spdlog::info("[app] prepare");


    create_pipeline();
    create_uniform_buffer();
    init_descriptor_set();


    // 初始化 per frame payload
    _payloads.resize(engine.frame_manager().frames().size());
    for (auto& _payload: _payloads)
    {
        FramePayload payload;
        payload.command_buffer = engine.device().command_pool().command_buffer_create(1).front();
        _payload               = payload;
    }
}


void Hello::App::create_pipeline()
{

    // pipelien layout
    _pipeline_layout = engine.vkdevice().createPipelineLayout({
            .setLayoutCount = 1,
            .pSetLayouts    = &_descriptor_set_layout,
    });


    _pipeline_template = Hiss::PipelineTemplate{
            .shader_stages        = {engine.shader_loader().load(shader_vert_path2, vk::ShaderStageFlagBits::eVertex),
                                     engine.shader_loader().load(shader_frag_path2, vk::ShaderStageFlagBits::eFragment)},
            .vertex_bindings      = Hiss::Vertex2DColor::get_binding_description(0),
            .vertex_attributes    = Hiss::Vertex2DColor::get_attribute_description(0),
            .color_attach_formats = {engine.color_format()},
            .depth_attach_format  = engine.depth_format(),
            .pipeline_layout      = _pipeline_layout,
            .dynamic_states       = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},
    };


    _pipeline = _pipeline_template.generate(engine.device());
}


void Hello::App::record_command(vk::CommandBuffer command_buffer, const FramePayload& payload, const Hiss::Frame& frame)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});


    // depth attachment execution barrier: 确保对 depth buffer 的写入不会乱序
    _depth_image->execution_barrier(
            command_buffer, {vk::PipelineStageFlagBits::eLateFragmentTests},
            {vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::AccessFlagBits::eDepthStencilAttachmentWrite});


    // color attachment layout transfer: undefined -> present （无需保留之前的内容）
    frame.image().transfer_layout(
            command_buffer, {vk::PipelineStageFlagBits::eTopOfPipe, vk::AccessFlags()},
            {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
            vk::ImageLayout::eColorAttachmentOptimal, true);

    color_attach_info.imageView = frame.image().vkview();

    /* 绘制过程 */
    command_buffer.beginRendering(render_info);
    {
        assert(_vertex_buffer && _index_buffer);
        command_buffer.bindVertexBuffers(0, {_vertex_buffer->vkbuffer()}, {0});
        command_buffer.bindIndexBuffer(_index_buffer->vkbuffer(), 0, vk::IndexType::eUint32);
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
        command_buffer.setViewport(0, engine.viewport());
        command_buffer.setScissor(0, engine.scissor());
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout, 0, {_descriptor_set}, {});
        command_buffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    }
    command_buffer.endRendering();


    /* color attachment layout transition: color -> present */
    frame.image().transfer_layout(
            command_buffer,
            {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
            {vk::PipelineStageFlagBits::eBottomOfPipe, {}}, vk::ImageLayout::ePresentSrcKHR);


    command_buffer.end();
}


void Hello::App::update() noexcept
{
    engine.frame_manager().acquire_frame();
    auto& frame   = engine.current_frame();
    auto& payload = _payloads[frame.frame_id()];

    /* draw */
    record_command(payload.command_buffer, payload, frame);

    engine.queue().submit_commands(
            {
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, frame.acquire_semaphore()},
            },
            {payload.command_buffer}, {frame.submit_semaphore()}, frame.insert_fence());

    engine.frame_manager().submit_frame();
}


void Hello::App::resize()
{
    // depth attachmetn
    delete _depth_image;
    _depth_image = engine.create_depth_attach(vk::SampleCountFlagBits::e1);

    // payload
}


void Hello::App::clean()
{
    spdlog::info("[app] clean");

    // 清理 payload
    for (auto& payload: _payloads)
        ;

    // 清理其他的
    delete _depth_image;
    delete _vertex_buffer;
    delete _index_buffer;
    delete _uniform_buffer;

    engine.device().vkdevice().destroy(_pipeline);
    engine.device().vkdevice().destroy(_pipeline_layout);
    engine.vkdevice().destroy(_descriptor_set_layout);
}


void Hello::App::init_descriptor_set()
{
    // 绑定 uniform buffer
    assert(_uniform_buffer);

    Hiss::Initial::descriptor_set_write(engine.vkdevice(), _descriptor_set,
                                        {
                                                {vk::DescriptorType::eUniformBuffer, {.buffer = _uniform_buffer}},
                                        });
}


void Hello::App::create_uniform_buffer()
{
    _uniform_buffer = new Hiss::UniformBuffer(engine.allocator, sizeof(UniformData));
    _uniform_buffer->memory_copy(&_ubo, sizeof(UniformData));
}

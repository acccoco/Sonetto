#include "msaa.hpp"
#include "run.hpp"

RUN(MSAA::App)


void MSAA::App::prepare()
{
    prepare_pipeline();

    payloads.resize(engine.frame_manager().frames_number());
    for (auto& payload: payloads)
    {
        payload.uniform_buffer = new Hiss::UniformBuffer(engine.allocator, sizeof(MSAA::UniformBlock));
        payload.command_buffer = engine.device().command_pool().command_buffer_create().front();
    }


    prepare_descriptor_set();
}


void MSAA::App::clean()
{
    spdlog::info("[MSAA] clean");

    delete color_attach;
    delete depth_attach;

    engine.vkdevice().destroy(pipeline);
    engine.vkdevice().destroy(pipeline_layout);
    engine.vkdevice().destroy(descriptor_layout);

    for (auto& payload: payloads)
    {
        delete payload.uniform_buffer;
    }
}


void MSAA::App::prepare_pipeline()
{
    // pipeline layout
    pipeline_layout = engine.device().vkdevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = 1,
            .pSetLayouts    = &descriptor_layout,
    });


    // pipeline
    auto _pipeline_state = Hiss::PipelineTemplate{
            .shader_stages        = {engine.shader_loader().load(vert_shader_path, vk::ShaderStageFlagBits::eVertex),
                                     engine.shader_loader().load(frag_shader_path, vk::ShaderStageFlagBits::eFragment)},
            .vertex_bindings      = Hiss::Vertex3DColorUv::input_binding_description(0),
            .vertex_attributes    = Hiss::Vertex3DColorUv::input_attribute_description(0),
            .color_attach_formats = {engine.color_format()},
            .depth_attach_format  = engine.depth_format(),
            .pipeline_layout      = pipeline_layout,
            .dynamic_states       = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},
    };
    _pipeline_state.set_msaa(msaa_sample);
    pipeline = _pipeline_state.generate(engine.device());
}


void MSAA::App::update() noexcept
{
    engine.frame_manager().acquire_frame();
    auto& frame   = engine.current_frame();
    auto& payload = payloads[frame.frame_id()];


    /* record command */
    update_uniform(*payload.uniform_buffer);
    record_command(payload.command_buffer, payload, frame);

    // 绘制
    engine.queue().submit_commands({{vk::PipelineStageFlagBits::eColorAttachmentOutput, frame.acquire_semaphore()}},
                                   {payload.command_buffer}, {frame.submit_semaphore()}, frame.insert_fence());

    engine.frame_manager().submit_frame();
}


void MSAA::App::prepare_descriptor_set()
{
    assert(!payloads.empty());
    assert(descriptor_layout);

    /* descriptor set create */
    for (auto& payload: payloads)
        payload.descriptor_set = engine.create_descriptor_set(descriptor_layout);


    /* bind uniform buffer and descriptor set */
    for (auto& payload: payloads)
    {
        Hiss::Initial::descriptor_set_write(
                engine.vkdevice(), payload.descriptor_set,
                {
                        {vk::DescriptorType::eUniformBuffer, {.buffer = payload.uniform_buffer}},    // 1
                        {vk::DescriptorType::eCombinedImageSampler, {.tex = &tex}},                  // 2
                });
    }
}


void MSAA::App::record_command(vk::CommandBuffer command_buffer, const MSAA::Payload& payload, const Hiss::Frame& frame)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});


    // execution barrier
    depth_attach->execution_barrier(
            command_buffer, {vk::PipelineStageFlagBits::eLateFragmentTests},
            {vk::PipelineStageFlagBits::eEarlyFragmentTests,
             vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead});
    color_attach->execution_barrier(
            command_buffer, {vk::PipelineStageFlagBits::eColorAttachmentOutput},
            {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite});


    // layout transfer
    frame.image().transfer_layout(
            command_buffer, {vk::PipelineStageFlagBits::eTopOfPipe},
            {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
            vk::ImageLayout::eColorAttachmentOptimal, true);


    // render
    color_attach_info.resolveImageView = frame.image().vkview();

    command_buffer.beginRendering(render_info);
    {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        command_buffer.setViewport(0, engine.viewport());
        command_buffer.setScissor(0, engine.scissor());

        command_buffer.bindVertexBuffers(0, {mesh.vertex_buffer().vkbuffer()}, {0});
        command_buffer.bindIndexBuffer(mesh.index_buffer().vkbuffer(), 0, vk::IndexType::eUint32);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                                          {payload.descriptor_set}, {});

        command_buffer.drawIndexed(static_cast<uint32_t>(mesh.index_buffer().index_num), 1, 0, 0, 0);
    }
    command_buffer.endRendering();


    // layout transfer
    frame.image().transfer_layout(
            command_buffer,
            {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
            {vk::PipelineStageFlagBits::eBottomOfPipe}, vk::ImageLayout::ePresentSrcKHR);

    command_buffer.end();
}


void MSAA::App::update_uniform(Hiss::UniformBuffer& uniform_buffer)
{
    assert(!payloads.empty() && payloads[0].uniform_buffer);

    float time = (float) engine.timer().now_ms() / 1000.f;

    UniformBlock ubo = {
            .model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f)),
            .view  = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f)),
            .proj = Hiss::perspective(45.f, (float) engine.extent().width / (float) engine.extent().height, 0.1f, 10.f),
    };


    uniform_buffer.memory_copy(&ubo, sizeof(ubo));
}
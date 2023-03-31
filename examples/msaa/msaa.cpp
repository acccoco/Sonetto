#include "msaa.hpp"
#include "run.hpp"

RUN(MSAA::App)


void MSAA::App::prepare()
{
    prepare_pipeline();

    payloads.resize(engine.frame_manager().frames_number());
    for (auto& payload: payloads)
    {
        payload.uniform_buffer = std::make_shared<Hiss::Buffer>(
                engine.device(), engine.allocator, sizeof(MSAA::UniformBlock),
                vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, "uniform buffer");
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
}


void MSAA::App::prepare_pipeline()
{
    // pipeline layout
    pipeline_layout = Hiss::Initial::pipeline_layout(engine.vkdevice(), {descriptor_layout, material_layout->layout});


    // pipeline
    auto _pipeline_state = Hiss::PipelineTemplate{
            .shader_stages        = {engine.shader_loader().load(vert_shader_path, vk::ShaderStageFlagBits::eVertex),
                                     engine.shader_loader().load(frag_shader_path, vk::ShaderStageFlagBits::eFragment)},
            .vertex_bindings      = Hiss::Vertex3D::binding_description(0),
            .vertex_attributes    = Hiss::Vertex3D::attribute_description(0),
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
    auto& frame   = engine.current_frame();
    auto& payload = payloads[frame.frame_id()];


    /* record command */
    record_command(payload.command_buffer, payload, frame);

    // 绘制
    engine.queue().submit_commands({}, {payload.command_buffer}, {frame.submit_semaphore()}, frame.insert_fence());
}


void MSAA::App::prepare_descriptor_set()
{
    assert(!payloads.empty());
    assert(descriptor_layout);

    /* descriptor set create */
    for (auto& payload: payloads)
        payload.descriptor_set = engine.create_descriptor_set(descriptor_layout, "");


    /* bind uniform buffer and descriptor set */
    for (auto& payload: payloads)
    {
        Hiss::Initial::descriptor_set_write(
                engine.vkdevice(), payload.descriptor_set,
                {
                        {.type = vk::DescriptorType::eUniformBuffer, .buffer = payload.uniform_buffer.get()},    // 1
                });
    }
}


void MSAA::App::record_command(vk::CommandBuffer command_buffer, const MSAA::Payload& payload, const Hiss::Frame& frame)
{
    float time = (float) engine.timer().now_ms() / 1000.f;
    ubo.model  = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f));

    command_buffer.begin(vk::CommandBufferBeginInfo{});

    command_buffer.updateBuffer(payload.uniform_buffer->vkbuffer(), 0, sizeof(ubo), &ubo);

    payload.uniform_buffer->memory_barrier(command_buffer,
                                           {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite},
                                           {vk::PipelineStageFlagBits::eVertexShader, vk::AccessFlagBits::eShaderRead});


    // execution barrier
    depth_attach->execution_barrier(
            command_buffer, {vk::PipelineStageFlagBits::eLateFragmentTests},
            {vk::PipelineStageFlagBits::eEarlyFragmentTests,
             vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead});


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

        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                                          {payload.descriptor_set}, {});

        mesh2.root_node->draw([this, command_buffer](const Hiss::MatMesh& mesh, const glm::mat4& matrix) {
            // 绑定纹理
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 1,
                                              {mesh.mat->descriptor_set->vk_descriptor_set}, {});


            command_buffer.bindVertexBuffers(0, {mesh.mesh->vertex_buffer->vkbuffer()}, {0});
            command_buffer.bindIndexBuffer(mesh.mesh->index_buffer->vkbuffer(), 0, vk::IndexType::eUint32);
            command_buffer.drawIndexed((uint32_t) mesh.mesh->index_buffer->index_num, 1, 0, 0, 0);
        });

        // 绘制立方体
        //        command_buffer.bindVertexBuffers(0, {mesh2_cube.vertex_buffer().vkbuffer()}, {0});
        //        command_buffer.bindIndexBuffer(mesh2_cube.index_buffer().vkbuffer(), 0, vk::IndexType::eUint32);
        //        command_buffer.drawIndexed(static_cast<uint32_t>(mesh2_cube.index_buffer().index_num), 1, 0, 0, 0);
    }
    command_buffer.endRendering();

    Hiss::Engine::color_attach_layout_trans_2(command_buffer, frame.image());


    command_buffer.end();
}

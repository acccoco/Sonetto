#pragma once
#include "forward_common.hpp"


namespace ForwardPlus
{
struct FinalPass : public Hiss::IPass
{
    explicit FinalPass(Hiss::Engine& engine, Resource& resource)
        : Hiss::IPass(engine),
          resource(resource)
    {}

    Resource& resource;


    const std::filesystem::path shader_vert = shader / "forward_plus" / "final.vert";
    const std::filesystem::path shader_frag = shader / "forward_plus" / "final.frag";

    // 默认的纹理，用于占位
    Hiss::Texture tex{engine.device(), engine.allocator, texture / "head.jpg", vk::Format::eR8G8B8A8Srgb};

    vk::DescriptorSetLayout descriptor_set_layout_0;
    vk::DescriptorSetLayout descriptor_set_layout_1;
    vk::DescriptorSetLayout descriptor_set_layout_2;
    vk::PipelineLayout      pipeline_layout;
    vk::Pipeline            pipeline;


    // framebuffer 中 depth 的配置，非常重要！！！！
    vk::RenderingAttachmentInfo depth_attach_info = vk::RenderingAttachmentInfo{
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eDontCare,
            .clearValue  = vk::ClearValue{.depthStencil{1.f, 0}},
    };
    vk::RenderingAttachmentInfo color_attach_info = Hiss::Initial::color_attach_info();

    vk::RenderingInfo render_info =
            Hiss::Initial::render_info(color_attach_info, depth_attach_info, g_engine->extent());


    struct Payload
    {
        vk::CommandBuffer command_buffer = g_engine->device().create_commnad_buffer("final-pass");
        vk::DescriptorSet descriptor_set_0;
        vk::DescriptorSet descriptor_set_1;
        vk::DescriptorSet descriptor_set_2;
    };

    std::vector<Payload> payloads{g_engine->frame_manager().frames_number()};


    void create_descriptor_layout()
    {
        // vertex shader 的 descriptor set layout
        descriptor_set_layout_0 = Hiss::Initial::descriptor_set_layout(
                g_engine->vkdevice(), {
                                              {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},
                                              {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},
                                      });

        // fragment shader 的表示材质的 set layout
        descriptor_set_layout_1 = Hiss::Initial::descriptor_set_layout(
                g_engine->vkdevice(),
                {
                        // binding 0: material
                        {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment},
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                });

        // fragment shader 的用于光照计算的 set layout
        descriptor_set_layout_2 = Hiss::Initial::descriptor_set_layout(
                g_engine->vkdevice(), {
                                              {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment},
                                              {vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eFragment},
                                              {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment},
                                      });
    }


    void create_pipeline()
    {
        // 创建 pipeline layout
        pipeline_layout = Hiss::Initial::pipeline_layout(
                g_engine->vkdevice(),
                {
                        descriptor_set_layout_0,
                        descriptor_set_layout_1,
                        descriptor_set_layout_2,
                },
                {
                        vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)},
                });


        // 创建 pipeline：将深度写入设为小于等于，减少 overdraw
        Hiss::PipelineTemplate pipeline_template = {
                .shader_stages        = {g_engine->shader_loader().load(shader_vert, vk::ShaderStageFlagBits::eVertex),
                                         g_engine->shader_loader().load(shader_frag, vk::ShaderStageFlagBits::eFragment)},
                .vertex_bindings      = Hiss::Vertex3DNormalUV::input_binding_description(),
                .vertex_attributes    = Hiss::Vertex3DNormalUV::input_attribute_description(),
                .color_attach_formats = {g_engine->color_format()},
                .depth_attach_format  = g_engine->depth_format(),
                .pipeline_layout      = pipeline_layout,
        };
        pipeline_template.set_viewport(g_engine->extent());
        pipeline = pipeline_template.generate(g_engine->device());
    }


    void bind_descriptor_set()
    {
        // 将 descriptor set 与 buffer 绑定起来
        for (int i = 0; i < payloads.size(); ++i)
        {
            auto& payload        = payloads[i];
            auto& shared_payload = resource.payloads[i];


            // vertex 使用的
            Hiss::Initial::descriptor_set_write(
                    g_engine->vkdevice(), payload.descriptor_set_0,
                    {
                            {.type = vk::DescriptorType::eUniformBuffer, .buffer = &shared_payload.perframe_uniform},
                            {.type = vk::DescriptorType::eUniformBuffer, .buffer = &resource.scene_uniform},
                    });

            //fragment 的材质
            Hiss::Initial::descriptor_set_write(
                    g_engine->vkdevice(), payload.descriptor_set_1,
                    {
                            {.type = vk::DescriptorType::eUniformBuffer, .buffer = &resource.material_uniform},
                            {.type    = vk::DescriptorType::eCombinedImageSampler,
                             .image   = &tex.image(),
                             .sampler = tex.sampler()},
                            {.type    = vk::DescriptorType::eCombinedImageSampler,
                             .image   = &tex.image(),
                             .sampler = tex.sampler()},
                            {.type    = vk::DescriptorType::eCombinedImageSampler,
                             .image   = &tex.image(),
                             .sampler = tex.sampler()},
                            {.type    = vk::DescriptorType::eCombinedImageSampler,
                             .image   = &tex.image(),
                             .sampler = tex.sampler()},
                    });

            // fragment 用到的光源相关
            Hiss::Initial::descriptor_set_write(
                    g_engine->vkdevice(), payload.descriptor_set_2,
                    {
                            {.type = vk::DescriptorType::eStorageBuffer, .buffer = &shared_payload.light_index_ssbo},
                            {.type = vk::DescriptorType::eStorageImage, .image = &shared_payload.light_grid_ssio},
                            {.type = vk::DescriptorType::eStorageBuffer, .buffer = &resource.lights_ssbo},
                    });
        }
    }


    void prepare()
    {
        create_descriptor_layout();
        create_pipeline();


        // 创建 descriptor set
        for (auto& payload: payloads)
        {
            payload.descriptor_set_0 = g_engine->create_descriptor_set(descriptor_set_layout_0, "final-pass-set-0");
            payload.descriptor_set_1 = g_engine->create_descriptor_set(descriptor_set_layout_1, "final-pass-set-1");
            payload.descriptor_set_2 = g_engine->create_descriptor_set(descriptor_set_layout_2, "final-pass-set-2");
        }

        bind_descriptor_set();
    }


    /**
     * 每一帧都需要录制命令
     */
    void record_command(vk::CommandBuffer& command_buffer, Hiss::Frame& frame)
    {
        auto& payload = payloads[frame.frame_id()];

        auto& shared_payload = resource.payloads[frame.frame_id()];


        // 更新 framebuffer 的信息
        depth_attach_info.imageView = shared_payload.depth_attach.vkview();
        color_attach_info.imageView = frame.image().vkview();


        command_buffer.begin(vk::CommandBufferBeginInfo{});
        {
            // color attach 进行 layout trans
            frame.image().memory_barrier(
                    {vk::PipelineStageFlagBits::eTopOfPipe},
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, command_buffer);


            // depth attache layout transfer：等待 light cull pass 完成深度的读取
            shared_payload.depth_attach.memory_barrier(
                    {vk::PipelineStageFlagBits::eComputeShader},
                    {vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
                     vk::AccessFlagBits::eDepthStencilAttachmentRead},
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, command_buffer);

            // 等待 light cull pass 写入 light index list
            shared_payload.light_index_ssbo.memory_barrier(
                    command_buffer, {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite},
                    {vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead});


            // 等待 light cull pass 写入 light index grid
            shared_payload.light_grid_ssio.memory_barrier(
                    {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite},
                    {vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead},
                    vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral, command_buffer);


            // 进行绘制
            command_buffer.beginRendering(render_info);
            {
                command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0u,
                                                  {
                                                          payload.descriptor_set_0,
                                                          payload.descriptor_set_1,
                                                          payload.descriptor_set_2,
                                                  },
                                                  {});

                command_buffer.bindVertexBuffers(0, {resource.mesh_cube.vertex_buffer().vkbuffer()}, {0});
                command_buffer.bindIndexBuffer(resource.mesh_cube.index_buffer().vkbuffer(), 0, vk::IndexType::eUint32);


                for (auto& mat: resource.cube_matrix)
                {
                    command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0,
                                                 sizeof(glm::mat4), &mat);
                    command_buffer.drawIndexed((uint32_t) resource.mesh_cube.index_buffer().index_num, 1, 0, 0, 0);
                }
            }
            command_buffer.endRendering();


            // 再对 color attach 进行 layout trans
            frame.image().memory_barrier(
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                    {vk::PipelineStageFlagBits::eBottomOfPipe}, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR, command_buffer);
        }
        command_buffer.end();
    }


    void update()
    {
        auto& frame   = g_engine->current_frame();
        auto& payload = payloads[frame.frame_id()];

        record_command(payload.command_buffer, frame);

        g_engine->queue().submit_commands({}, {payload.command_buffer}, {frame.submit_semaphore()},
                                          frame.insert_fence());
    }


    void clean() const
    {
        g_engine->vkdevice().destroy(descriptor_set_layout_0);
        g_engine->vkdevice().destroy(descriptor_set_layout_1);
        g_engine->vkdevice().destroy(descriptor_set_layout_2);
        g_engine->vkdevice().destroy(pipeline_layout);
        g_engine->vkdevice().destroy(pipeline);
    }
};

}    // namespace ForwardPlus
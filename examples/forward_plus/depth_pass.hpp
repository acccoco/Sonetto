#pragma once
#include "forward_common.hpp"


namespace ForwardPlus
{


/**
 * 该 pass 只为得到场景的深度信息，只需要 depth attach 即可
 */
struct DepthPass : public Hiss::IPass
{
    explicit DepthPass(Hiss::Engine& engine, Resource& resource)
        : Hiss::IPass(engine),
          resource(resource)
    {}

    Resource& resource;

    const std::filesystem::path shader_depth_vert = shader / "forward_plus" / "depth.vert";
    const std::filesystem::path shader_depth_frag = shader / "forward_plus" / "depth.frag";

    vk::DescriptorSetLayout descriptor_set_layout;
    vk::PipelineLayout      pipeline_layout;
    vk::Pipeline            pipeline;

    // framebuffer 中 depth attach 的设置，确保 store op 是 store
    vk::RenderingAttachmentInfo depth_attach_info = vk::RenderingAttachmentInfo{
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
    };
    vk::RenderingInfo render_info = Hiss::Initial::render_info(depth_attach_info, g_engine->extent());


    struct FramePayload
    {
        vk::CommandBuffer command_buffer = g_engine->device().create_commnad_buffer("depth-pass");
        vk::DescriptorSet descriptor_set;
    };

    std::vector<FramePayload> payloads{g_engine->frame_manager().frames_number()};


    void prepare()
    {
        descriptor_set_layout = Hiss::Initial::descriptor_set_layout(
                g_engine->vkdevice(), {
                                              // binding 0: perframe uniform
                                              {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},
                                              // binding 1: scene uniform
                                              {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},
                                      });


        // 使用 push constant 的方式向 vertex shader 传入 model matrix
        pipeline_layout = Hiss::Initial::pipeline_layout(g_engine->vkdevice(), {descriptor_set_layout},
                                                         {{vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)}});


        Hiss::PipelineTemplate pipe_template = {
                .shader_stages   = {g_engine->shader_loader().load(shader_depth_vert, vk::ShaderStageFlagBits::eVertex),
                                    g_engine->shader_loader().load(shader_depth_frag,
                                                                   vk::ShaderStageFlagBits::eFragment)},
                .vertex_bindings = Hiss::Vertex3DNormalUV::input_binding_description(),
                .vertex_attributes   = Hiss::Vertex3DNormalUV::input_attribute_description(),
                .depth_attach_format = g_engine->depth_format(),
                .pipeline_layout     = pipeline_layout,
        };
        pipe_template.set_viewport(g_engine->extent());
        pipeline = pipe_template.generate(g_engine->device());


        // 创建 descirptor set
        for (auto& payload: payloads)
        {
            payload.descriptor_set = g_engine->create_descriptor_set(descriptor_set_layout, "depth-pass");
        }


        // 将 descriptor 与 buffer 绑定起来
        for (int i = 0; i < payloads.size(); ++i)
        {
            Hiss::Initial::descriptor_set_write(
                    g_engine->vkdevice(), payloads[i].descriptor_set,
                    {
                            // binding 0: perframe uniform
                            {.type   = vk::DescriptorType::eUniformBuffer,
                             .buffer = &resource.payloads[i].perframe_uniform},
                            // binding 1: scene uniform
                            {.type = vk::DescriptorType::eUniformBuffer, .buffer = &resource.scene_uniform},
                    });
        }
    }


    /**
     * 每一帧都需要录制命令
     */
    void record_command(vk::CommandBuffer& command_buffer, Hiss::Frame& frame)
    {
        depth_attach_info.imageView = resource.payloads[frame.frame_id()].depth_attach.vkview();

        // depth 等资源没有被占用，无需同步

        command_buffer.begin(vk::CommandBufferBeginInfo{});
        {
            command_buffer.beginRendering(render_info);
            {
                command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                                                  {payloads[frame.frame_id()].descriptor_set}, {});

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
        }
        command_buffer.end();
    }


    std::vector<bool> has_run = std::vector<bool>(engine.frame_manager().frames_number(), false);

    void update()
    {
        // 直接执行命令
        auto& frame   = g_engine->current_frame();
        auto& payload = payloads[frame.frame_id()];

//        if (has_run[frame.frame_id()])
//            return;
//        else
//            has_run[frame.frame_id()] = true;


        record_command(payload.command_buffer, frame);

        g_engine->queue().submit_commands({}, {payload.command_buffer}, {}, frame.insert_fence());
    }


    void clean() const
    {
        g_engine->vkdevice().destroy(descriptor_set_layout);
        g_engine->vkdevice().destroy(pipeline_layout);
        g_engine->vkdevice().destroy(pipeline);
    }
};

}    // namespace ForwardPlus
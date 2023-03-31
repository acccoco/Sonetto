#pragma once
#include "bloom_common.hpp"
#include "utils/application.hpp"
#include "proj_config.hpp"
#include "utils/vk_func.hpp"
#include "utils/pipeline_template.hpp"
#include "engine/model2.hpp"
#include "utils/post_process.hpp"


namespace Bloom
{
class CombinePass : public Hiss::IPass
{
public:
    struct Resource
    {
        std::shared_ptr<Hiss::Image2D> hdr_image;
        std::shared_ptr<Hiss::Image2D> bloom_image;

        Hiss::Image2D* color_output{};
    };


    explicit CombinePass(Hiss::Engine& engine)
        : Hiss::IPass(engine)
    {}

    void prepare(const std::vector<CombinePass::Resource>& resources)
    {
        assert(resources.size() == payloads.size());
        for (int i = 0; i < payloads.size(); ++i)
            payloads[i].resouece = resources[i];


        create_descriptor();
        create_pipeline();
        create_framebuffer();

        for (auto&& payload: payloads)
        {
            record_command(payload);
        }
    }


    void update()
    {
        auto&& frame   = engine.current_frame();
        auto&& payload = payloads[frame.frame_id()];

        frame.submit_command(payload.command_buffer);
    }


private:
    struct Payload
    {
        Bloom::CombinePass::Resource resouece;
        vk::CommandBuffer            command_buffer = g_engine->device().create_commnad_buffer("combine pass");
        vk::DescriptorSet            descriptor_set;
        vk::RenderingAttachmentInfo  color_attach_info;
        vk::RenderingInfo            rendering_info;
    };


    std::vector<Payload>    payloads{engine.frame_manager().frames_number()};
    vk::Pipeline            pipeline;
    vk::PipelineLayout      pipeline_layout;
    vk::DescriptorSetLayout descriptor_layout;
    vk::Sampler             sampler = Hiss::Initial::sampler(engine.device());

    std::shared_ptr<Hiss::PostProcess::VertexBuffer> vertex_buffer = Hiss::PostProcess::vertex_buffer(engine);

    std::filesystem::path vert_shader_path = shader / "shader" / "post_process.vert";
    std::filesystem::path frag_shader_path = shader / "bloom" / "combine.frag";


    void create_descriptor()
    {
        descriptor_layout = Hiss::Initial::descriptor_set_layout(
                engine.vkdevice(), {{vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment}});

        for (auto& payload: payloads)
        {
            payload.descriptor_set = engine.create_descriptor_set(descriptor_layout, "combine pass");
        }

        for (auto& payload: payloads)
        {
            Hiss::Initial::descriptor_set_write(engine.vkdevice(), payload.descriptor_set,
                                                {{.type    = vk::DescriptorType::eCombinedImageSampler,
                                                  .image   = payload.resouece.hdr_image.get(),
                                                  .sampler = sampler},
                                                 {.type    = vk::DescriptorType::eCombinedImageSampler,
                                                  .image   = payload.resouece.bloom_image.get(),
                                                  .sampler = sampler}});
        }
    }

    void create_pipeline()
    {
        pipeline_layout = Hiss::Initial::pipeline_layout(engine.vkdevice(), {descriptor_layout});

        auto vert_shader_stage = engine.shader_loader().load(vert_shader_path, vk::ShaderStageFlagBits::eVertex);
        auto frag_shader_stage = engine.shader_loader().load(frag_shader_path, vk::ShaderStageFlagBits::eFragment);

        Hiss::PipelineTemplate pipeline_template = {
                .shader_stages        = {vert_shader_stage, frag_shader_stage},
                .vertex_bindings      = Hiss::PostProcess::binding_descriptions,
                .vertex_attributes    = Hiss::PostProcess::attribute_descriptions,
                .color_attach_formats = {engine.color_format()},
                .pipeline_layout      = pipeline_layout,
        };
        pipeline_template.set_viewport(engine.extent());
        pipeline_template.depth_stencil_state.setDepthTestEnable(VK_FALSE);
        pipeline = pipeline_template.generate(engine.device());
    }


    void create_framebuffer()
    {
        for (auto&& payload: payloads)
        {
            payload.color_attach_info           = Hiss::Initial::color_attach_info();
            payload.color_attach_info.imageView = payload.resouece.color_output->vkview();

            payload.rendering_info = vk::RenderingInfo{
                    .renderArea           = {.offset = {0, 0}, .extent = engine.extent()},
                    .layerCount           = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments    = &payload.color_attach_info,
            };
        }
    }


    void record_command(Payload& payload)
    {
        auto&& command_buffer = payload.command_buffer;
        command_buffer.begin(vk::CommandBufferBeginInfo{});
        {
            command_buffer.beginRendering(payload.rendering_info);
            {
                command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                                                  {payload.descriptor_set}, {});
                command_buffer.bindVertexBuffers(0, {vertex_buffer->vkbuffer()}, {0});
                command_buffer.draw(Hiss::PostProcess::vertices.size(), 1, 0, 0);
            }
            command_buffer.endRendering();
        }
        command_buffer.end();
    }
};

}    // namespace Bloom
#pragma once
#include "bloom_common.hpp"
#include "utils/application.hpp"
#include "proj_config.hpp"
#include "utils/vk_func.hpp"
#include "utils/pipeline_template.hpp"
#include "engine/model2.hpp"


namespace Bloom
{
class ColorPass : public Hiss::IPass
{
public:
    struct Resource
    {
        std::shared_ptr<Hiss::Image2D>     hdr_color;
        std::shared_ptr<Hiss::DepthAttach> depth_attach;
        std::shared_ptr<Hiss::Buffer>      frame_uniform;
        std::shared_ptr<Hiss::Buffer>      scene_uniform;
        std::shared_ptr<Hiss::Buffer>      light_ssbo;
    };

    explicit ColorPass(Hiss::Engine& engine)
        : Hiss::IPass(engine)
    {}

    void prepare(const std::vector<ColorPass::Resource>& resources)
    {
        assert(resources.size() == payloads.size());
        for (int i = 0; i < resources.size(); ++i)
            payloads[i].resource = resources[i];

        create_descriptor();
        create_pipeline();
    }

    void update(Hiss::ModelNode& model_nodel)
    {
        auto&& frame   = engine.current_frame();
        auto&& payload = payloads[frame.frame_id()];
    }

    void clean()
    {
        engine.vkdevice().destroy(pipeline);
        engine.vkdevice().destroy(pipeline_layout);
        engine.vkdevice().destroy(descriptor_layout0);
        engine.vkdevice().destroy(descriptor_layout2);
    }


private:
    struct Payload
    {
        vk::DescriptorSet   descriptor_set0;
        vk::DescriptorSet   descriptor_set2;
        ColorPass::Resource resource;
        vk::CommandBuffer   command_buffer = g_engine->device().create_commnad_buffer("color pass");
    };


    vk::Pipeline            pipeline;
    vk::PipelineLayout      pipeline_layout;
    vk::DescriptorSetLayout descriptor_layout0;
    vk::DescriptorSetLayout descriptor_layout2;
    std::vector<Payload>    payloads{engine.frame_manager().frames_number()};

    const std::filesystem::path vert_shader = shader / "bloom" / "common.vert";
    const std::filesystem::path frag_shader = shader / "bloom" / "common.frag";

    // framebuffer 的配置
    vk::RenderingAttachmentInfo color_attach_info = Hiss::Initial::color_attach_info();
    vk::RenderingAttachmentInfo depth_attach_info = Hiss::Initial::depth_attach_info();
    vk::RenderingInfo render_info = Hiss::Initial::render_info(color_attach_info, depth_attach_info, engine.extent());


    void create_descriptor()
    {
        descriptor_layout0 = Hiss::Initial::descriptor_set_layout(
                engine.vkdevice(), {{vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},
                                    {vk::DescriptorType::eUniformBuffer,
                                     vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment}});

        descriptor_layout2 = Hiss::Initial::descriptor_set_layout(
                engine.vkdevice(), {{vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment}});


        for (auto&& paylaod: payloads)
        {
            paylaod.descriptor_set0 = engine.create_descriptor_set(descriptor_layout0, "color pass descriptor 0");
            paylaod.descriptor_set2 = engine.create_descriptor_set(descriptor_layout2, "color pass descriptor 2");
        }

        for (auto&& payload: payloads)
        {
            Hiss::Initial::descriptor_set_write(
                    engine.vkdevice(), payload.descriptor_set0,
                    {{.type = vk::DescriptorType::eUniformBuffer, .buffer = payload.resource.frame_uniform.get()},
                     {.type = vk::DescriptorType::eUniformBuffer, .buffer = payload.resource.scene_uniform.get()}});

            Hiss::Initial::descriptor_set_write(
                    engine.vkdevice(), payload.descriptor_set2,
                    {{.type = vk::DescriptorType::eStorageBuffer, .buffer = payload.resource.light_ssbo.get()}});
        }
    }


    void create_pipeline()
    {
        pipeline_layout = Hiss::Initial::pipeline_layout(
                engine.vkdevice(),
                {
                        descriptor_layout0,
                        engine.material_layout,
                        descriptor_layout2,
                },
                {
                        vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)},
                });

        auto shader_stage_vert = engine.shader_loader().load(vert_shader, vk::ShaderStageFlagBits::eVertex);
        auto shader_stage_frag = engine.shader_loader().load(frag_shader, vk::ShaderStageFlagBits::eFragment);
        auto pipeline_template = Hiss::PipelineTemplate{
                .shader_stages        = {shader_stage_vert, shader_stage_frag},
                .vertex_bindings      = Hiss::Vertex3D::binding_description(0),
                .vertex_attributes    = Hiss::Vertex3D::attribute_description(0),
                .color_attach_formats = {payloads.front().resource.hdr_color->format()},
                .depth_attach_format  = payloads.front().resource.depth_attach->format(),
                .pipeline_layout      = pipeline_layout,
        };
        pipeline_template.set_viewport(engine.extent());

        pipeline = pipeline_template.generate(engine.device());
    }


    void record_command(Payload& payload, Hiss::Frame& frame, Hiss::ModelNode& model_node)
    {
        auto&& command_buffer = payload.command_buffer;

        // 更新 framebuffer 的信息
        color_attach_info.imageView = payload.resource.hdr_color->vkview();
        depth_attach_info.imageView = payload.resource.depth_attach->vkview();

        command_buffer.begin(vk::CommandBufferBeginInfo{});
        {
            command_buffer.beginRendering({});
            {
                command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                                                  {payload.descriptor_set0}, {});
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 2,
                                                  {payload.descriptor_set2}, {});


                model_node.draw([this, command_buffer](const Hiss::MatMesh& mesh, const glm::mat4& matrix) {
                    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 1,
                                                      {mesh.mat->descriptor_set}, {});
                    command_buffer.bindVertexBuffers(0, {mesh.mesh->vertex_buffer->vkbuffer()}, {0});
                    command_buffer.bindIndexBuffer(mesh.mesh->index_buffer->vkbuffer(), 0, vk::IndexType::eUint32);
                    command_buffer.drawIndexed((uint32_t) mesh.mesh->index_buffer->index_num, 1, 0, 0, 0);
                });
            }
            command_buffer.endRendering();
        }
        command_buffer.end();
    }
};
}    // namespace Bloom
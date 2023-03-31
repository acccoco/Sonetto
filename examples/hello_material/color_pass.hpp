#pragma once
#include "utils/application.hpp"
#include "proj_config.hpp"
#include "utils/vk_func.hpp"
#include "utils/pipeline_template.hpp"
#include "engine/model2.hpp"
#include "utils/descriptor.hpp"


namespace Material
{
class ColorPass : public Hiss::IPass
{
public:
    struct Resource
    {
        std::shared_ptr<Hiss::Image2D> depth_attach;

        std::shared_ptr<Hiss::Buffer> frame_ubo;
        std::shared_ptr<Hiss::Buffer> scene_ubo;

        std::shared_ptr<Hiss::Buffer> light_ssbo;
    };

    explicit ColorPass(Hiss::Engine& engine, const std::vector<Resource>& resources)
        : Hiss::IPass(engine)
    {
        assert(resources.size() == payloads.size());
        for (int i = 0; i < payloads.size(); ++i)
        {
            payloads[i].resource       = resources[i];
            payloads[i].command_buffer = engine.device().create_commnad_buffer("color pass command buffer");
        }

        create_descriptor();
        create_pipeline();
        create_framebuffer();
    }


    void update(Hiss::ModelNode& model_node)
    {
        auto& frame   = engine.current_frame();
        auto& payload = payloads[frame.frame_id()];

        record_command(payload, model_node);
        frame.submit_command(payload.command_buffer);
    }


    ~ColorPass() override
    {
        engine.vkdevice().destroy(pipeline);
        engine.vkdevice().destroy(pipeline_layout);
    }


private:
    struct Payload
    {
        Resource resource;

        vk::RenderingAttachmentInfo color_attach_info;
        vk::RenderingAttachmentInfo depth_attach_info;
        vk::RenderingInfo           rendering_info;
        vk::CommandBuffer           command_buffer;

        std::shared_ptr<Hiss::DescriptorSet> set_0;
        std::shared_ptr<Hiss::DescriptorSet> set_2;
    };

    const std::filesystem::path vert_path = shader / "hello_material" / "hello_material.vert";
    const std::filesystem::path frag_path = shader / "hello_material" / "hello_material.frag";

    std::vector<Payload> payloads{engine.frame_manager().frames_number()};

    vk::Pipeline                            pipeline;
    vk::PipelineLayout                      pipeline_layout;
    std::shared_ptr<Hiss::DescriptorLayout> layout_0;
    std::shared_ptr<Hiss::DescriptorLayout> layout_2;


    void create_descriptor()
    {
        layout_0 = std::make_shared<Hiss::DescriptorLayout>(
                engine.device(), std::vector<Hiss::Initial::BindingInfo>{
                                         {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},
                                         {vk::DescriptorType::eUniformBuffer,
                                          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment}});

        layout_2 = std::make_shared<Hiss::DescriptorLayout>(
                engine.device(), std::vector<Hiss::Initial::BindingInfo>{
                                         {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment}});


        for (auto& payload: payloads)
        {
            payload.set_0 = std::make_shared<Hiss::DescriptorSet>(engine, layout_0, "layout set 0");
            payload.set_2 = std::make_shared<Hiss::DescriptorSet>(engine, layout_2, "layout set 2");

            payload.set_0->write({
                    {.buffer = payload.resource.frame_ubo.get()},
                    {.buffer = payload.resource.scene_ubo.get()},
            });

            payload.set_2->write({
                    {.buffer = payload.resource.light_ssbo.get()},
            });
        }
    }

    void create_pipeline()
    {
        pipeline_layout = Hiss::Initial::pipeline_layout(
                engine.vkdevice(),
                {layout_0->layout, Hiss::Matt::get_material_descriptor(engine.device())->layout, layout_2->layout},
                {vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)}});

        auto vert_shader_stage = engine.shader_loader().load(vert_path, vk::ShaderStageFlagBits::eVertex);
        auto frag_shader_stage = engine.shader_loader().load(frag_path, vk::ShaderStageFlagBits::eFragment);

        Hiss::PipelineTemplate pipeline_template = {
                .shader_stages        = {vert_shader_stage, frag_shader_stage},
                .vertex_bindings      = Hiss::Vertex3D::binding_description(0),
                .vertex_attributes    = Hiss::Vertex3D::attribute_description(0),
                .color_attach_formats = {engine.color_format()},
                .depth_attach_format  = payloads.front().resource.depth_attach->format(),
                .pipeline_layout      = pipeline_layout,
        };
        pipeline_template.set_viewport(engine.extent());
        pipeline = pipeline_template.generate(engine.device());
    }

    void create_framebuffer()
    {
        for (int i = 0; i < payloads.size(); ++i)
        {
            auto& payload = payloads[i];

            payload.color_attach_info           = Hiss::Initial::color_attach_info();
            payload.color_attach_info.imageView = engine.frame_manager().frame(i).image().vkview();

            payload.depth_attach_info = Hiss::Initial::depth_attach_info(payload.resource.depth_attach->vkview());

            payload.rendering_info =
                    Hiss::Initial::render_info(payload.color_attach_info, payload.depth_attach_info, engine.extent());
        }
    }

    void record_command(Payload& payload, Hiss::ModelNode& model_node)
    {
        auto& command_buffer = payload.command_buffer;

        command_buffer.begin(vk::CommandBufferBeginInfo{});
        {
            command_buffer.beginRendering(payload.rendering_info);
            {
                command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                                                  payload.set_0->vk_descriptor_set, {});
                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 2,
                                                  payload.set_2->vk_descriptor_set, {});

                model_node.draw([&](const Hiss::MatMesh& mat_mesh, const glm::mat4& matrix) {
                    // 绑定纹理
                    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 1,
                                                      mat_mesh.mat->descriptor_set->vk_descriptor_set, {});

                    // 绑定顶点属性
                    command_buffer.bindVertexBuffers(0, {mat_mesh.mesh->vertex_buffer->vkbuffer()}, {0});
                    command_buffer.bindIndexBuffer(mat_mesh.mesh->index_buffer->vkbuffer(), 0, vk::IndexType::eUint32);

                    // 传入 push constant
                    command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(matrix),
                                                 &matrix);

                    // 绘制
                    command_buffer.drawIndexed((uint32_t) mat_mesh.mesh->index_buffer->index_num, 1, 0, 0, 0);
                });
            }
            command_buffer.endRendering();
        }
        command_buffer.end();
    }
};

}    // namespace Material
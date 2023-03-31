#pragma once
#include "utils/application.hpp"
#include "proj_config.hpp"
#include "utils/vk_func.hpp"
#include "utils/pipeline_template.hpp"
#include "engine/model2.hpp"


/**
 * 存放一些模版代码，使用的时候直接 copy
 */


namespace Temp
{


class ColorPass : public Hiss::IPass
{
public:
    struct Resource
    {
        std::shared_ptr<Hiss::Image2D> depth_attach;
    };

    explicit ColorPass(Hiss::Engine& engine)
        : Hiss::IPass(engine)
    {}

    void prepare(const std::vector<Resource>& resources)
    {
        assert(resources.size() == payloads.size());
        for (int i = 0; i < payloads.size(); ++i)
        {
            payloads[i].resource = resources[i];
        }

        create_descriptor();
        create_pipeline();
        create_framebuffer();
        // TODO
    }

    void update(Hiss::ModelNode& model_node)
    {
        auto& frame   = engine.current_frame();
        auto& payload = payloads[frame.frame_id()];

        frame.submit_command(payload.command_buffer);
        // TODO
    }

    void clean()
    {
        engine.vkdevice().destroy(pipeline);
        engine.vkdevice().destroy(pipeline_layout);
        engine.vkdevice().destroy(descriptor_layout);
        // TODO
    }

private:
    struct Payload
    {
        Resource resource;

        vk::DescriptorSet           descriptor_set;
        vk::RenderingAttachmentInfo color_attach_info;
        vk::RenderingAttachmentInfo depth_attach_info;
        vk::RenderingInfo           rendering_info;
        vk::CommandBuffer           command_buffer;
    };

    const std::filesystem::path vert_path = shader / "";
    const std::filesystem::path frag_path = shader / "";

    std::vector<Payload> payloads{engine.frame_manager().frames_number()};

    vk::Pipeline            pipeline;
    vk::PipelineLayout      pipeline_layout;
    vk::DescriptorSetLayout descriptor_layout;

    void create_descriptor()
    {
        // TODO
    }

    void create_pipeline()
    {
        pipeline_layout = Hiss::Initial::pipeline_layout(engine.vkdevice(), {descriptor_layout});

        auto vert_shader_stage = engine.shader_loader().load(vert_path, vk::ShaderStageFlagBits::eVertex);
        auto frag_shader_stage = engine.shader_loader().load(frag_path, vk::ShaderStageFlagBits::eFragment);

        Hiss::PipelineTemplate pipeline_template = {
                .shader_stages = {vert_shader_stage, frag_shader_stage},
                // TODO .vertex_bindings = ,
                // TODO .vertex_attributes = ,
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
                                                  {payload.descriptor_set}, {});

                // command_buffer.bindVertexBuffers
                // command_buffer.bindIndexBuffers
                // command_buffer.draw()
                // model_node.draw()
            }
            command_buffer.endRendering();
        }
        command_buffer.end();
        // TODO
    }
};


class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {
        for (auto& payload: payloads)
        {
            payload.depth_attach = Hiss::Image2D::create_depth_attach(engine.allocator, engine.device(),
                                                                      engine.depth_format(), engine.extent());
        }
        // TODO
    }

    ColorPass color_pass{engine};

    void prepare() override
    {
        // 初始化 color pass
        std::vector<ColorPass::Resource> color_pass_resources{engine.frame_manager().frames_number()};
        for (int i = 0; i < engine.frame_manager().frames_number(); ++i)
        {
            color_pass_resources[i] = {
                    .depth_attach = payloads[i].depth_attach,
                    // TODO
            };
        }

        color_pass.prepare(color_pass_resources);

        // TODO
    }

    void update() override
    {
        auto& frame   = engine.current_frame();
        auto& payload = payloads[frame.frame_id()];

        // color attach: layout trans
        {
            Hiss::Frame::FrameCommandBuffer command_buffer(frame, "pre color pass");
            frame.image().memory_barrier(
                    {vk::PipelineStageFlagBits::eTopOfPipe}, {vk::PipelineStageFlagBits::eColorAttachmentOutput},
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, command_buffer());
        }

        // color_pass.update();

        // color attach: layout trans
        {
            Hiss::Frame::FrameCommandBuffer command_buffer(frame, "post color pass");
            frame.image().memory_barrier(
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                    {vk::PipelineStageFlagBits::eBottomOfPipe}, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR, command_buffer());
        }
    }

    void clean() override
    {
        color_pass.clean();
        // TODO
    }


private:
    struct Payload
    {
        std::shared_ptr<Hiss::Image2D> depth_attach;
    };

    std::vector<Payload> payloads{engine.frame_manager().frames_number()};
};
}    // namespace Temp
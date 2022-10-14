#pragma once
#include "core/engine.hpp"
#include "proj_config.hpp"
#include "func/texture.hpp"
#include "func/pipeline_template.hpp"
#include "application.hpp"
#include "vk_config.hpp"
#include "func/vk_func.hpp"

#include "./particle.hpp"


namespace ParticleCompute
{
struct Graphics
{

#pragma region layout 部分
    struct UBO    // std140
    {
        alignas(16) glm::mat4 projection;
        alignas(16) glm::mat4 view;
        alignas(16) glm::vec2 screen_dim;
    };

    std::vector<vk::VertexInputBindingDescription> vertex_input_bindings = {
            {0, sizeof(Particle), vk::VertexInputRate::eVertex},
    };


    std::vector<vk::VertexInputAttributeDescription> vertex_attribute_bindings = {
            {0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, pos)},
            {1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, vel)},
    };

    const std::filesystem::path vert_shader_path  = shader / "compute_particle/particle.vert";
    const std::filesystem::path frag_shader_path  = shader / "compute_particle/particle.frag";
    const std::filesystem::path tex_particle_path = texture / "compute_particle/particle_rgba.png";
    const std::filesystem::path tex_gradient_path = texture / "compute_particle/particle_gradient_rgba.png";


    const std::vector<vk::DescriptorSetLayoutBinding> graphics_descriptor_bindings = {
            {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
            {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
            {2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex},
    };

#pragma endregion


    struct Payload
    {
        Hiss::UniformBuffer* uniform_buffer = nullptr;
        vk::DescriptorSet    descriptor_set = VK_NULL_HANDLE;
        vk::CommandBuffer    command_buffer = VK_NULL_HANDLE;
    };


    Hiss::PipelineTemplate  pipeline_template     = {};
    vk::DescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    vk::PipelineLayout      pipeline_layout       = VK_NULL_HANDLE;
    vk::Pipeline            pipeline              = VK_NULL_HANDLE;
    UBO                     ubo                   = {};


    std::vector<Payload> payloads;


    Hiss::Texture* tex_particle = nullptr;
    Hiss::Texture* tex_gradient = nullptr;


    Hiss::Image2D* depth_image = nullptr;


    Hiss::Buffer2* storage_buffer = nullptr;
    uint32_t       num_particles  = 0;

    Hiss::Engine& engine;


    Graphics(Hiss::Engine& engine, Hiss::Buffer2* storage_buffer, uint32_t num_particles)
        : storage_buffer(storage_buffer),
          num_particles(num_particles),
          engine(engine)
    {}


#pragma region 对外接口
public:
    void prepare()
    {
        spdlog::info("graphics prepare");

        create_pipeline();

        // 初始化每一帧的数据
        payloads.resize(engine.frame_manager().frames_number());
        for (int i = 0; i < payloads.size(); ++i)
        {
            auto& payload = payloads[i];

            // 注意 semaphore 应该不是 signaled
            payload.command_buffer =
                    engine.device().command_pool().command_buffer_create(fmt::format("graphics[{}]", i));
        }

        depth_image = engine.create_depth_attach(vk::SampleCountFlagBits::e1);

        load_assets();
        create_uniform_buffer();
        prepare_descriptor_set();
    }


    void update()
    {
        auto& frame   = engine.current_frame();
        auto& payload = payloads[frame.frame_id()];


        update_uniform_buffer(*payload.uniform_buffer);

        // 重新录制命令
        vk::CommandBuffer command_buffer = payload.command_buffer;
        command_buffer.reset();
        record_command(command_buffer, frame, payload);


        // 绘制
        engine.queue().submit_commands(
                {
                        {vk::PipelineStageFlagBits::eColorAttachmentOutput, frame.acquire_semaphore()},
                },
                {command_buffer}, {frame.submit_semaphore()}, frame.insert_fence());
    }


    void clean()
    {
        spdlog::info("graphics clean");

        DELETE(tex_particle);
        DELETE(tex_gradient);

        DELETE(depth_image);


        engine.vkdevice().destroy(descriptor_set_layout);
        engine.vkdevice().destroy(pipeline_layout);
        engine.vkdevice().destroy(pipeline);
        for (auto& payload: payloads)
        {
            DELETE(payload.uniform_buffer);
        }
    }


    void resize()
    {
        delete depth_image;
        depth_image = engine.create_depth_attach(vk::SampleCountFlagBits::e1);
    }
#pragma endregion


#pragma region 初始化以及更新的方法
private:
    // 读取纹理资源
    void load_assets()
    {
        tex_particle = new Hiss::Texture(engine.device(), engine.allocator, tex_particle_path,
                                         vk::Format::eR8G8B8A8Srgb, false);
        tex_gradient = new Hiss::Texture(engine.device(), engine.allocator, tex_gradient_path,
                                         vk::Format::eR8G8B8A8Srgb, false);
    }


    // 创建 uniform buffer
    void create_uniform_buffer()
    {
        update_ubo();
        assert(!payloads.empty());

        for (auto& payload: payloads)
        {
            payload.uniform_buffer = new Hiss::UniformBuffer(engine.allocator, sizeof(ubo));
            payload.uniform_buffer->memory_copy(&ubo, sizeof(ubo));
        }
    }


    void create_pipeline()
    {
        /* descriptor set layout */
        vk::DescriptorSetLayoutCreateInfo descriptor_info;
        descriptor_info.setBindings(graphics_descriptor_bindings);
        descriptor_set_layout = engine.vkdevice().createDescriptorSetLayout(descriptor_info);


        /* pipeline layout */
        pipeline_layout = engine.vkdevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 1,
                .pSetLayouts    = &descriptor_set_layout,
        });


        // pipeline 的其他设置
        pipeline_template = Hiss::PipelineTemplate{
                .shader_stages = {engine.shader_loader().load(vert_shader_path, vk::ShaderStageFlagBits::eVertex),
                                  engine.shader_loader().load(frag_shader_path, vk::ShaderStageFlagBits::eFragment)},

                .vertex_bindings      = vertex_input_bindings,
                .vertex_attributes    = vertex_attribute_bindings,
                .color_attach_formats = {engine.color_format()},
                .depth_attach_format  = engine.depth_format(),
                .pipeline_layout      = pipeline_layout,

                .dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},

                .depth_stencil_state =
                        vk::PipelineDepthStencilStateCreateInfo{.depthTestEnable  = VK_FALSE,
                                                                .depthWriteEnable = VK_FALSE,
                                                                .depthCompareOp   = vk::CompareOp::eAlways,
                                                                .back = {.compareOp = vk::CompareOp::eAlways}},
        };
        pipeline_template.assembly_state.topology = vk::PrimitiveTopology::ePointList;


        /* additive blend */
        // 混合关闭，否则太亮了
        //        pipeline_template.color_blend_state = vk::PipelineColorBlendAttachmentState{
        //                .blendEnable         = VK_TRUE,
        //                .srcColorBlendFactor = vk::BlendFactor::eOne,
        //                .dstColorBlendFactor = vk::BlendFactor::eOne,
        //                .colorBlendOp        = vk::BlendOp::eAdd,
        //                .srcAlphaBlendFactor = vk::BlendFactor::eSrcAlpha,
        //                .dstAlphaBlendFactor = vk::BlendFactor::eDstAlpha,
        //                .alphaBlendOp        = vk::BlendOp::eAdd,
        //                .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
        //                                | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        //        };


        pipeline = pipeline_template.generate(engine.device());
    }


    // 创建 descriptor_set，将 descriptor_set 与 buffer 绑定起来
    void prepare_descriptor_set()
    {

        /* 创建 descriptor sets */
        std::vector<vk::DescriptorSetLayout> set_layouts(payloads.size(), descriptor_set_layout);
        auto descriptor_sets = engine.vkdevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                .descriptorPool     = engine.descriptor_pool(),
                .descriptorSetCount = static_cast<uint32_t>(set_layouts.size()),
                .pSetLayouts        = set_layouts.data(),
        });
        for (int i = 0; i < payloads.size(); ++i)
            payloads[i].descriptor_set = descriptor_sets[i];


        /* 将 descriptor set 与 buffer，image 绑定起来 */
        vk::DescriptorImageInfo particle_image_descriptor = {
                .sampler     = tex_particle->sampler(),
                .imageView   = tex_particle->image().view().vkview,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        vk::DescriptorImageInfo gradient_image_descriptor = {
                .sampler     = tex_gradient->sampler(),
                .imageView   = tex_gradient->image().view().vkview,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        for (auto& payload: payloads)
        {
            vk::DescriptorBufferInfo buffer_descriptor = {payload.uniform_buffer->vkbuffer(), 0, VK_WHOLE_SIZE};

            engine.vkdevice().updateDescriptorSets(
                    {
                            vk::WriteDescriptorSet{.dstSet          = payload.descriptor_set,
                                                   .dstBinding      = 0,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                                                   .pImageInfo      = &particle_image_descriptor},
                            vk::WriteDescriptorSet{.dstSet          = payload.descriptor_set,
                                                   .dstBinding      = 1,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                                                   .pImageInfo      = &gradient_image_descriptor},
                            vk::WriteDescriptorSet{.dstSet          = payload.descriptor_set,
                                                   .dstBinding      = 2,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType  = vk::DescriptorType::eUniformBuffer,
                                                   .pBufferInfo     = &buffer_descriptor},
                    },
                    {});
        }
    };


    void record_command(vk::CommandBuffer command_buffer, Hiss::Frame& frame, Graphics::Payload& payload) const
    {
        command_buffer.begin(vk::CommandBufferBeginInfo{});

        // execution barrier: depth attachment
        depth_image->execution_barrier(
                command_buffer, {vk::PipelineStageFlagBits::eLateFragmentTests},
                {vk::PipelineStageFlagBits::eEarlyFragmentTests,
                 vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite});


        // layout transition: color attachment 不需要之前的数据
        frame.image().transfer_layout(
                command_buffer, {vk::PipelineStageFlagBits::eTopOfPipe, {}},
                {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                vk::ImageLayout::eColorAttachmentOptimal, true);


        // pipeline barrier: storage buffer
        assert(storage_buffer);
        storage_buffer->memory_barrier(
                command_buffer, {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite},
                {vk::PipelineStageFlagBits::eVertexInput, vk::AccessFlagBits::eVertexAttributeRead});


        // framebuffer 的信息
        auto color_attach_info = vk::RenderingAttachmentInfo{
                .imageView   = frame.image().view().vkview,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp      = vk::AttachmentLoadOp::eClear,
                .storeOp     = vk::AttachmentStoreOp::eStore,
                .clearValue  = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.136f, 0.122f, 0.111f, 1.f}}},
        };
        auto depth_attach_info = vk::RenderingAttachmentInfo{
                .imageView   = depth_image->view().vkview,
                .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .loadOp      = vk::AttachmentLoadOp::eClear,
                .storeOp     = vk::AttachmentStoreOp::eDontCare,
                .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
        };

        /* render pass */
        command_buffer.beginRendering(vk::RenderingInfo{
                .renderArea           = {.extent = engine.extent()},
                .layerCount           = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &color_attach_info,
                .pDepthAttachment     = &depth_attach_info,
        });

        command_buffer.setViewport(0, {vk::Viewport{
                                              0,
                                              0,
                                              (float) engine.extent().width,
                                              (float) engine.extent().height,
                                              0.f,
                                              1.f,
                                      }});
        command_buffer.setScissor(0, {vk::Rect2D{.offset = {0, 0}, .extent = engine.extent()}});
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
                                          {payload.descriptor_set}, {});
        command_buffer.bindVertexBuffers(0, {storage_buffer->vkbuffer()}, {0});
        command_buffer.draw(num_particles, 1, 0, 0);
        command_buffer.endRendering();


        // layout transition: color attachment
        frame.image().transfer_layout(
                command_buffer,
                {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                {vk::PipelineStageFlagBits::eBottomOfPipe}, vk::ImageLayout::ePresentSrcKHR, false);


        command_buffer.end();
    }


    void update_uniform_buffer(Hiss::UniformBuffer& buffer)
    {
        update_ubo();
        buffer.memory_copy(&ubo, sizeof(ubo));
    }


    // 更新 struct ubo，里面的 projmatrix 之类的
    void update_ubo()
    {
        ubo.projection = glm::perspective(
                glm::radians(60.f),
                static_cast<float>(engine.extent().width) / static_cast<float>(engine.extent().height), 0.1f, 256.f);
        ubo.projection[1][1] *= -1.f;
        ubo.view = glm::lookAt(glm::vec3(8.f, 8.f, 8.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
        ubo.screen_dim =
                glm::vec2(static_cast<float>(engine.extent().width), static_cast<float>(engine.extent().height));
    }
#pragma endregion
};

}    // namespace ParticleCompute
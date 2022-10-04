#include "compute_shader_Nbody.hpp"
#include "utils/tools.hpp"
#include "utils/rand.hpp"
#include "run.hpp"


RUN(NBody::App)


void NBody::App::prepare()
{

    /* 公共部分初始化 */
    particles_prepare();
    storage_buffer_prepare();


    graphics_prepare();
    compute_prepare();
}


void NBody::App::clean()
{
    spdlog::info("[NBody] clean");

    compute_clean();
    graphics_clean();


    DELETE(storage_buffer);
}


void NBody::App::resize()
{
    graphics_resize();

    spdlog::debug("depth size: ({}, {})", graphics.depth_image->extent().width, graphics.depth_image->extent().height);
}


void NBody::App::compute_prepare()
{
    spdlog::info("[NBody] compute prepare");
    assert(num_particles);

    /* workgroup 相关的数据 */
    compute.workgroup_size = std::min<uint32_t>(256, engine.gpu().properties().limits.maxComputeWorkGroupSize[0]);
    compute.shared_data_size =
            std::min<uint32_t>(1024, engine.gpu().properties().limits.maxComputeSharedMemorySize / sizeof(glm::vec4));
    compute.workgroup_num = num_particles / compute.workgroup_size;
    spdlog::info("[NBody] workgroup size: {}, workgroup count: {}, shared data size: {}", compute.workgroup_size,
                 compute.workgroup_num, compute.shared_data_size);


    // 初始化每一帧需要用到的数据
    compute.payloads = {engine.frame_manager().frames().size(), Compute::Payload()};
    for (int i = 0; i < compute.payloads.size(); ++i)
    {
        auto& payload = compute.payloads[i];

        // 注意 semaphore 应该是 signaled
        payload.command_buffer = engine.device().command_pool().command_buffer_create(fmt::format("compute[{}]", i));
    }


    compute_uniform_buffer_prepare();
    compute_prepare_descriptor_set();
    compute_pipeline_prepare();


    /* 录制 command buffer */
    for (auto& payload: compute.payloads)
        compute_record_command(payload.command_buffer, payload.descriptor_set);
}


void NBody::App::graphics_prepare()
{
    spdlog::info("[NBody] graphics prepare");

    // 初始化每一帧的数据
    graphics.payloads.resize(engine.frame_manager().frames_number());
    for (int i = 0; i < graphics.payloads.size(); ++i)
    {
        auto& payload = graphics.payloads[i];

        // 注意 semaphore 应该不是 signaled
        payload.command_buffer = engine.device().command_pool().command_buffer_create(fmt::format("graphics[{}]", i));
    }

    graphics.depth_image = engine.create_depth_image();

    graphics_load_assets();
    graphics_create_uniform_buffer();
    graphics_prepare_descriptor_set();
    graphics_pipeline_prepare();
}


/**
 * 创建两个 compute pipeline
 */
void NBody::App::compute_pipeline_prepare()
{
    /* pipeline layout */
    compute.pipeline_layout = engine.vkdevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = 1,
            .pSetLayouts    = &compute.descriptor_set_layout,
    });


    /* 1st pipeline: calculate */
    {
        /* shader, and specialization constant */
        std::array<vk::SpecializationMapEntry, 5> specialization_entries = {{
                {0, offsetof(MovementSpecializationData, workgroup_size),
                 sizeof(compute.specialization_data.workgroup_size)},
                {1, offsetof(MovementSpecializationData, shared_data_size),
                 sizeof(compute.specialization_data.shared_data_size)},
                {2, offsetof(MovementSpecializationData, gravity), sizeof(compute.specialization_data.gravity)},
                {3, offsetof(MovementSpecializationData, power), sizeof(compute.specialization_data.power)},
                {4, offsetof(MovementSpecializationData, soften), sizeof(compute.specialization_data.soften)},
        }};

        compute.specialization_data.workgroup_size   = compute.workgroup_size;
        compute.specialization_data.shared_data_size = compute.shared_data_size;

        vk::SpecializationInfo specialization_info = {
                static_cast<uint32_t>(specialization_entries.size()),
                specialization_entries.data(),
                sizeof(compute.specialization_data),
                &compute.specialization_data,
        };
        vk::ComputePipelineCreateInfo pipeline_info = {
                .stage  = engine.shader_loader().load(compute.shader_file_calculate, vk::ShaderStageFlagBits::eCompute),
                .layout = compute.pipeline_layout,
        };
        pipeline_info.stage.pSpecializationInfo = &specialization_info;

        /* generate pipeline */
        vk::Result result;
        std::tie(result, compute.pipeline_calculate) =
                engine.vkdevice().createComputePipeline(VK_NULL_HANDLE, pipeline_info);
        vk::resultCheck(result, "failed to create compute pipeline: calculate.");
    }


    /* 2nd pipeline: integrate */
    {
        /* shader, and specialization */
        vk::SpecializationMapEntry specialization_entry = {0, 0, sizeof(compute.workgroup_size)};
        vk::SpecializationInfo     specialization_info  = {
                1,
                &specialization_entry,
                sizeof(compute.workgroup_size),
                &compute.workgroup_size,
        };

        vk::ComputePipelineCreateInfo pipeline_info = {
                .stage  = engine.shader_loader().load(compute.shader_file_integrate, vk::ShaderStageFlagBits::eCompute),
                .layout = compute.pipeline_layout,
        };
        pipeline_info.stage.pSpecializationInfo = &specialization_info;

        /* generate pipeline */
        vk::Result result;
        std::tie(result, compute.pipeline_intergrate) =
                engine.vkdevice().createComputePipeline(VK_NULL_HANDLE, pipeline_info);
        vk::resultCheck(result, "failed to create compute pipeline: integrate");
    }
}


/**
 * 创建 descriptor set，并将其绑定到 uniform buffer 和 storage buffer
 */
void NBody::App::compute_prepare_descriptor_set()
{
    /* descriptor set layout */
    compute.descriptor_set_layout = engine.vkdevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(compute_descriptor_bindings.size()),
            .pBindings    = compute_descriptor_bindings.data(),
    });


    /* descriptor sets */
    std::vector<vk::DescriptorSetLayout> temp_layout(compute.payloads.size(), compute.descriptor_set_layout);
    auto descriptor_sets = engine.vkdevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool     = engine.descriptor_pool(),
            .descriptorSetCount = static_cast<uint32_t>(temp_layout.size()),
            .pSetLayouts        = temp_layout.data(),
    });
    for (int i = 0; i < compute.payloads.size(); ++i)
        compute.payloads[i].descriptor_set = descriptor_sets[i];


    /* 将 descriptor 和 buffer 绑定起来 */
    assert(storage_buffer);
    for (auto& payload: compute.payloads)
    {
        vk::DescriptorBufferInfo storage_buffer_info = {storage_buffer->vkbuffer(), 0, VK_WHOLE_SIZE};
        vk::DescriptorBufferInfo uniform_buffer_info = {payload.uniform_buffer->vkbuffer(), 0, VK_WHOLE_SIZE};

        std::array<vk::WriteDescriptorSet, 2> descriptor_writes = {{
                {
                        .dstSet          = payload.descriptor_set,
                        .dstBinding      = 0,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType  = vk::DescriptorType::eStorageBuffer,
                        .pBufferInfo     = &storage_buffer_info,
                },
                {
                        .dstSet          = payload.descriptor_set,
                        .dstBinding      = 1,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType  = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo     = &uniform_buffer_info,
                },
        }};
        engine.vkdevice().updateDescriptorSets(descriptor_writes, {});
    }
}


/**
 * 录制命令
 */
void NBody::App::compute_record_command(vk::CommandBuffer command_buffer, vk::DescriptorSet descriptor_set)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});


    // 从 graphics 阶段获取 storage buffer
    assert(storage_buffer);
    storage_buffer->memory_barrier(command_buffer,
                                   {vk::PipelineStageFlagBits::eVertexInput, vk::AccessFlagBits::eVertexAttributeRead},
                                   {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite});


    /* 1st pass: 计算受力，更新速度 */
    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute.pipeline_calculate);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute.pipeline_layout, 0, {descriptor_set},
                                      nullptr);
    // IMPL constant 是什么时候绑定的？
    command_buffer.dispatch(compute.workgroup_num, 1, 1);


    /* 在 1st 和 2nd 之间插入 memory barrier */
    storage_buffer->memory_barrier(command_buffer,
                                   {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite},
                                   {vk::PipelineStageFlagBits::eComputeShader,
                                    vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite});


    /* 2nd pass: 更新质点的位置 */
    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute.pipeline_intergrate);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute.pipeline_layout, 0, {descriptor_set},
                                      nullptr);
    command_buffer.dispatch(compute.workgroup_num, 1, 1);


    command_buffer.end();
}


void NBody::App::particles_prepare()
{
    spdlog::info("[NBody] particles craete");

    std::vector<glm::vec3> attractors = {
            glm::vec3(5.0f, 0.0f, 0.0f),     //
            glm::vec3(-5.0f, 0.0f, 0.0f),    //
            glm::vec3(0.0f, 0.0f, 5.0f),     //
            glm::vec3(0.0f, 0.0f, -5.0f),    //
            glm::vec3(0.0f, 4.0f, 0.0f),     //
            glm::vec3(0.0f, -8.0f, 0.0f),    //
    };
    spdlog::info("[NBody] number of attractors: {}", attractors.size());

    num_particles = attractors.size() * PARTICLES_PER_ATTRACTOR;
    particles     = std::vector<Particle>(num_particles);

    Hiss::Rand rand;

    /* initial particle position and velocity */
    for (uint32_t attr_idx = 0; attr_idx < static_cast<uint32_t>(attractors.size()); ++attr_idx)
    {
        for (uint32_t p_in_group_idx = 0; p_in_group_idx < PARTICLES_PER_ATTRACTOR; ++p_in_group_idx)
        {
            Particle& particle = particles[attr_idx * PARTICLES_PER_ATTRACTOR + p_in_group_idx];

            /* 每一组中的第一个 particle 是重心 */
            if (p_in_group_idx == 0)
            {
                particle.pos = glm::vec4(attractors[attr_idx] * 1.5f, 90000.f);
                particle.vel = glm::vec4(0.f);
            }
            else
            {
                /* position */
                glm::vec3 position = attractors[attr_idx]
                                   + 0.75f * glm::vec3(rand.normal_0_1(), rand.normal_0_1(), rand.normal_0_1());
                float len = glm::length(glm::normalize(position - attractors[attr_idx]));
                position.y *= 2.f - (len * len);

                /* velocity */
                glm::vec3 angular  = glm::vec3(0.5f, 1.5f, 0.5f) * (attr_idx % 2 == 0 ? 1.f : -1.f);
                glm::vec3 velocity = glm::cross((position - attractors[attr_idx]), angular)
                                   + glm::vec3(rand.normal_0_1(), rand.normal_0_1(), rand.normal_0_1() * 0.025f);

                float mass   = (rand.normal_0_1() * 0.5f + 0.5f) * 75.f;
                particle.pos = glm::vec4(position, mass);
                particle.vel = glm::vec4(velocity, 0.f);
            }


            /* 颜色渐变的 uv 坐标 */
            particle.vel.w = (float) attr_idx * 1.f / (float) attractors.size();
        }
    }
}


void NBody::App::storage_buffer_prepare()
{
    assert(!particles.empty());
    spdlog::info("[NBody] storage buffer prepare");


    /* create storage buffer */
    vk::DeviceSize storage_buffer_size = particles.size() * sizeof(Particle);

    storage_buffer    = new Hiss::Buffer2(engine.allocator, storage_buffer_size,
                                          vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
                                                  | vk::BufferUsageFlagBits::eStorageBuffer,
                                          VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    auto stage_buffer = Hiss::StageBuffer(engine.allocator, storage_buffer_size);
    stage_buffer.mem_copy(particles.data(), storage_buffer_size);


    /* stage buffer -> storage buffer */
    Hiss::OneTimeCommand command_buffer(engine.device(), engine.device().command_pool());
    command_buffer().copyBuffer(stage_buffer.vkbuffer(), storage_buffer->vkbuffer(),
                                {vk::BufferCopy{.size = storage_buffer_size}});


    command_buffer.exec();
}


void NBody::App::graphics_create_uniform_buffer()
{
    graphics_update_uniform_data();

    for (auto& payload: graphics.payloads)
    {
        payload.uniform_buffer = new Hiss::UniformBuffer(engine.allocator, sizeof(graphics.ubo));
        payload.uniform_buffer->memory_copy(&graphics.ubo, sizeof(graphics.ubo));
    }
}


void NBody::App::compute_uniform_buffer_prepare()
{
    assert(num_particles);

    /* init ubo */
    compute.ubo.particle_count = static_cast<int32_t>(num_particles);
    compute.ubo.delta_time     = 0.f;


    /* init uniform buffers */
    for (auto& payload: compute.payloads)
    {
        payload.uniform_buffer = new Hiss::UniformBuffer(engine.allocator, sizeof(compute.ubo));
        payload.uniform_buffer->memory_copy(&compute.ubo, sizeof(compute.ubo));
    }
}


void NBody::App::graphics_pipeline_prepare()
{
    /* shader stage */
    graphics.pipeline_template.shader_stages.push_back(
            engine.shader_loader().load(graphics.vert_shader_path, vk::ShaderStageFlagBits::eVertex));
    graphics.pipeline_template.shader_stages.push_back(
            engine.shader_loader().load(graphics.frag_shader_path, vk::ShaderStageFlagBits::eFragment));


    /* vertex, index and assembly */
    graphics.pipeline_template.vertex_bindings.push_back({vk::VertexInputBindingDescription{
            .binding   = 0,
            .stride    = sizeof(Particle),
            .inputRate = vk::VertexInputRate::eVertex,
    }});
    graphics.pipeline_template.vertex_attributes = {
            vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, pos)},
            vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, vel)},
    };
    graphics.pipeline_template.assembly_state.topology = vk::PrimitiveTopology::ePointList;


    /* additive blend */
    graphics.pipeline_template.color_blend_state = vk::PipelineColorBlendAttachmentState{
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eOne,
            .colorBlendOp        = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstAlphaBlendFactor = vk::BlendFactor::eDstAlpha,
            .alphaBlendOp        = vk::BlendOp::eAdd,
            .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };


    /* depth test */
    graphics.pipeline_template.depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
            .depthTestEnable  = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp   = vk::CompareOp::eAlways,
            .back             = {.compareOp = vk::CompareOp::eAlways},
    };


    /* pipeline layout */
    assert(graphics.descriptor_set_layout);
    graphics.pipeline_template.pipeline_layout = graphics.pipeline_layout =
            engine.vkdevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
                    .setLayoutCount = 1,
                    .pSetLayouts    = &graphics.descriptor_set_layout,
            });


    // 设置 framebuffer
    graphics.pipeline_template.color_attach_formats = {engine.color_format()};
    graphics.pipeline_template.depth_attach_format  = engine.depth_format();


    /* dynamic state */
    graphics.pipeline_template.dynamic_states.push_back(vk::DynamicState::eViewport);
    graphics.pipeline_template.dynamic_states.push_back(vk::DynamicState::eScissor);


    graphics.pipeline = graphics.pipeline_template.generate(engine.device());
}


void NBody::App::graphics_prepare_descriptor_set()
{
    /* descriptor set layout */
    graphics.descriptor_set_layout = engine.vkdevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(graphics_descriptor_bindings.size()),
            .pBindings    = graphics_descriptor_bindings.data(),
    });


    /* 创建 descriptor sets */
    std::vector<vk::DescriptorSetLayout> set_layouts(graphics.payloads.size(), graphics.descriptor_set_layout);
    auto descriptor_sets = engine.vkdevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool     = engine.descriptor_pool(),
            .descriptorSetCount = static_cast<uint32_t>(set_layouts.size()),
            .pSetLayouts        = set_layouts.data(),
    });
    for (int i = 0; i < graphics.payloads.size(); ++i)
        graphics.payloads[i].descriptor_set = descriptor_sets[i];


    /* 将 descriptor set 与 buffer，image 绑定起来 */
    vk::DescriptorImageInfo particle_image_descriptor = {
            .sampler     = graphics.tex_particle->sampler(),
            .imageView   = graphics.tex_particle->image().view().vkview,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    vk::DescriptorImageInfo gradient_image_descriptor = {
            .sampler     = graphics.tex_gradient->sampler(),
            .imageView   = graphics.tex_gradient->image().view().vkview,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    for (auto& payload: graphics.payloads)
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
}


void NBody::App::graphics_load_assets()
{
    graphics.tex_particle = new Hiss::Texture(engine.device(), engine.allocator, graphics.tex_particle_path, false);
    graphics.tex_gradient = new Hiss::Texture(engine.device(), engine.allocator, graphics.tex_gradient_path, false);
}


void NBody::App::graphics_record_command(vk::CommandBuffer command_buffer, Hiss::Frame& frame,
                                         Graphics::Payload& payload)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});

    // execution barrier: depth attachment
    graphics.depth_image->execution_barrier(
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
    storage_buffer->memory_barrier(command_buffer,
                                   {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite},
                                   {vk::PipelineStageFlagBits::eVertexInput, vk::AccessFlagBits::eVertexAttributeRead});


    // framebuffer 的信息
    auto color_attach_info = vk::RenderingAttachmentInfo{
            .imageView   = frame.image().view().vkview,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.f}}},
    };
    auto depth_attach_info = vk::RenderingAttachmentInfo{
            .imageView   = graphics.depth_image->view().vkview,
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
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics.pipeline);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics.pipeline_layout, 0,
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


void NBody::App::update() noexcept
{
    double delta_time = engine.timer().duration_ms() / 1000.0;    // 以秒为单位

    engine.frame_manager().acquire_frame();
    auto& frame            = engine.current_frame();
    auto& graphics_payload = graphics.payloads[frame.frame_id()];
    auto& compute_payload  = compute.payloads[frame.frame_id()];

    /* update uniform buffers */
    compute_update_uniform(*compute_payload.uniform_buffer, (float) delta_time);
    graphics_update_uniform(*graphics_payload.uniform_buffer);


    /* record graphics command */
    vk::CommandBuffer graphics_command_buffer = graphics_payload.command_buffer;
    graphics_command_buffer.reset();
    graphics_record_command(graphics_command_buffer, frame, graphics_payload);


    /* graphics draw */
    engine.queue().submit_commands(
            {
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, frame.acquire_semaphore()},
            },
            {graphics_payload.command_buffer}, {frame.submit_semaphore()}, frame.insert_fence());


    engine.frame_manager().submit_frame();


    // compute 阶段，不用重新录制 command buffer
    engine.queue().submit_commands({}, {compute_payload.command_buffer}, {}, frame.insert_fence());
}


void NBody::App::compute_update_uniform(Hiss::UniformBuffer& buffer, float delta_time)
{
    compute.ubo.delta_time = delta_time;

    buffer.memory_copy(&compute.ubo, sizeof(compute.ubo));
}


void NBody::App::compute_clean()
{
    spdlog::info("[NBody] compute clean");


    engine.vkdevice().destroy(compute.descriptor_set_layout);
    engine.vkdevice().destroy(compute.pipeline_layout);
    engine.vkdevice().destroy(compute.pipeline_intergrate);
    engine.vkdevice().destroy(compute.pipeline_calculate);
    for (auto& payload: compute.payloads)
    {
        DELETE(payload.uniform_buffer);
    }
}


void NBody::App::graphics_clean()
{
    spdlog::info("[NBody] graphics clean");

    DELETE(graphics.tex_particle);
    DELETE(graphics.tex_gradient);

    DELETE(graphics.depth_image);


    engine.vkdevice().destroy(graphics.descriptor_set_layout);
    engine.vkdevice().destroy(graphics.pipeline_layout);
    engine.vkdevice().destroy(graphics.pipeline);
    for (auto& payload: graphics.payloads)
    {
        DELETE(payload.uniform_buffer);
    }
}


void NBody::App::graphics_update_uniform(Hiss::UniformBuffer& buffer)
{
    graphics_update_uniform_data();
    buffer.memory_copy(&graphics.ubo, sizeof(graphics.ubo));
}


void NBody::App::graphics_resize()
{
    delete graphics.depth_image;
    graphics.depth_image = engine.create_depth_image();
}


void NBody::App::graphics_update_uniform_data()
{
    graphics.ubo.projection = glm::perspective(
            glm::radians(60.f), static_cast<float>(engine.extent().width) / static_cast<float>(engine.extent().height),
            0.1f, 256.f);
    graphics.ubo.projection[1][1] *= -1.f;
    graphics.ubo.view = glm::lookAt(glm::vec3(8.f, 8.f, 8.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    graphics.ubo.screen_dim =
            glm::vec2(static_cast<float>(engine.extent().width), static_cast<float>(engine.extent().height));
}

#include "compute_shader_Nbody.hpp"
#include "utils/tools.hpp"
#include "utils/rand.hpp"
#include "run.hpp"


APP_RUN(ComputeShaderNBody)


void ComputeShaderNBody::prepare()
{
    Hiss::Engine::prepare();
    spdlog::info("[NBody] prepare");

    /* 公共部分初始化 */
    descriptor_pool_prepare();
    particles_prepare();
    storage_buffer_prepare();


    graphics_prepare();
    compute_prepare();
}


void ComputeShaderNBody::clean()
{
    spdlog::info("[NBody] clean");

    compute_clean();
    graphics_clean();


    DELETE(storage_buffer);
    _device->vkdevice().destroy(descriptor_pool);

    Hiss::Engine::clean();
}


void ComputeShaderNBody::resize()
{
    Hiss::Engine::resize();
}


void ComputeShaderNBody::compute_prepare()
{
    assert(num_particles);

    /* workgroup 相关的数据 */
    compute.workgroup_size = std::min<uint32_t>(256, _physical_device->properties().limits.maxComputeWorkGroupSize[0]);
    compute.shared_data_size = std::min<uint32_t>(1024, _physical_device->properties().limits.maxComputeSharedMemorySize
                                                                / sizeof(glm::vec4));
    compute.workgroup_num    = num_particles / compute.workgroup_size;
    spdlog::info("[NBody] workgroup size: {}, workgroup count: {}, shared data size: {}", compute.workgroup_size,
                  compute.workgroup_num, compute.shared_data_size);


    compute_uniform_buffer_prepare();
    compute_descriptor_prepare();
    compute_pipeline_prepare();


    /* 创建 semaphore，并设为 signaled 状态 */
    for (auto& semaphore: compute.semaphores)
    {
        semaphore = _device->create_semaphore(true);
    }


    /* 录制 command buffer */
    for (size_t i = 0; i < IN_FLIGHT_CNT; ++i)
    {
        compute_command_prepare(compute_command_buffer()[i], compute.descriptor_sets[i]);
    }
}


void ComputeShaderNBody::graphics_prepare()
{
    spdlog::info("[NBody] graphics prepare");

    graphics_load_assets();
    graphics_uniform_buffer_prepare();
    graphics_descriptor_set_prepare();
    graphics_pipeline_prepare();

    for (auto& semaphore: graphics.semaphores)
    {
        semaphore = _device->create_semaphore();
    }
}


void ComputeShaderNBody::descriptor_pool_prepare()
{
    /**
     * compute shader: 1 uniform buffer, 1 storage buffer
     * vertex shader: 1 uniform buffer
     * fragment shader: 2 sampler
     */
    std::array<vk::DescriptorPoolSize, 3> pool_size = {{
            {vk::DescriptorType::eUniformBuffer, 2 * IN_FLIGHT_CNT},
            {vk::DescriptorType::eStorageBuffer, 1 * IN_FLIGHT_CNT},
            {vk::DescriptorType::eCombinedImageSampler, 2 * IN_FLIGHT_CNT},
    }};

    /**
     * 只需要 2 个 descriptor set2
     *  compute 的 2 个 pipeline 共用一个 descriptor set
     *  graphics 只有 1 个 pipeline，只需要一个 descirptor set
     */
    descriptor_pool = _device->vkdevice().createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .maxSets       = 2 * IN_FLIGHT_CNT,
            .poolSizeCount = static_cast<uint32_t>(pool_size.size()),
            .pPoolSizes    = pool_size.data(),
    });
}


/**
 * 创建两个 compute pipeline
 */
void ComputeShaderNBody::compute_pipeline_prepare()
{
    /* pipeline layout */
    compute.pipeline_layout = _device->vkdevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
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
                .stage  = _shader_loader->load(compute.shader_file_calculate, vk::ShaderStageFlagBits::eCompute),
                .layout = compute.pipeline_layout,
        };
        pipeline_info.stage.pSpecializationInfo = &specialization_info;

        /* generate pipeline */
        vk::Result result;
        std::tie(result, compute.pipeline_calculate) =
                _device->vkdevice().createComputePipeline(VK_NULL_HANDLE, pipeline_info);
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
                .stage  = _shader_loader->load(compute.shader_file_integrate, vk::ShaderStageFlagBits::eCompute),
                .layout = compute.pipeline_layout,
        };
        pipeline_info.stage.pSpecializationInfo = &specialization_info;

        /* generate pipeline */
        vk::Result result;
        std::tie(result, compute.pipeline_intergrate) =
                _device->vkdevice().createComputePipeline(VK_NULL_HANDLE, pipeline_info);
        vk::resultCheck(result, "failed to create compute pipeline: integrate");
    }
}


/**
 * 创建 descriptor set，并将其绑定到 uniform buffer 和 storage buffer
 */
void ComputeShaderNBody::compute_descriptor_prepare()
{
    /* descriptor set layout */
    compute.descriptor_set_layout = _device->vkdevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(compute_descriptor_bindings.size()),
            .pBindings    = compute_descriptor_bindings.data(),
    });


    /* descriptor sets */
    std::vector<vk::DescriptorSetLayout> temp_layout(IN_FLIGHT_CNT, compute.descriptor_set_layout);
    compute.descriptor_sets = _device->vkdevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool     = descriptor_pool,
            .descriptorSetCount = static_cast<uint32_t>(temp_layout.size()),
            .pSetLayouts        = temp_layout.data(),
    });


    /* 将 descriptor 和 buffer 绑定起来 */
    for (size_t i = 0; i < IN_FLIGHT_CNT; ++i)
    {
        vk::DescriptorBufferInfo storage_buffer_info = {storage_buffer->vkbuffer(), 0, VK_WHOLE_SIZE};
        vk::DescriptorBufferInfo uniform_buffer_info = {compute.uniform_buffers[i]->vkbuffer(), 0, VK_WHOLE_SIZE};

        std::array<vk::WriteDescriptorSet, 2> descriptor_writes = {{
                {
                        .dstSet          = compute.descriptor_sets[i],
                        .dstBinding      = 0,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType  = vk::DescriptorType::eStorageBuffer,
                        .pBufferInfo     = &storage_buffer_info,
                },
                {
                        .dstSet          = compute.descriptor_sets[i],
                        .dstBinding      = 1,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType  = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo     = &uniform_buffer_info,
                },
        }};
        _device->vkdevice().updateDescriptorSets(descriptor_writes, {});
    }
}


/**
 * 录制命令
 */
void ComputeShaderNBody::compute_command_prepare(vk::CommandBuffer command_buffer, vk::DescriptorSet descriptor_set)
{
    assert(storage_buffer);
    command_buffer.begin(vk::CommandBufferBeginInfo{});
    bool need_queue_transfer = !Hiss::Queue::is_same_queue_family(_device->queue_compute(), _device->queue());


    /* queue family owner transfer: acquire */
    if (need_queue_transfer)
    {
        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eVertexShader,
                                       vk::PipelineStageFlagBits::eComputeShader, {}, {},
                                       {vk::BufferMemoryBarrier{
                                               .dstAccessMask       = vk::AccessFlagBits::eShaderWrite,
                                               .srcQueueFamilyIndex = _device->queue().family_index,
                                               .dstQueueFamilyIndex = _device->queue_compute().family_index,
                                               .buffer              = storage_buffer->vkbuffer(),
                                               .offset              = 0,
                                               .size                = storage_buffer->buffer_size(),
                                       }},
                                       {});
    }


    /* 1st pass: 计算受力，更新速度 */
    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute.pipeline_calculate);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute.pipeline_layout, 0, {descriptor_set},
                                      nullptr);
    // IMPL constant 是什么时候绑定的？
    command_buffer.dispatch(compute.workgroup_num, 1, 1);


    /* 在 1st 和 2nd 之间插入 memory barrier */
    command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {},
            {vk::BufferMemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                    .dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
                    .buffer        = storage_buffer->vkbuffer(),
                    .offset        = 0,
                    .size          = storage_buffer->buffer_size(),
            }},
            {});


    /* 2nd pass: 更新质点的位置 */
    command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute.pipeline_intergrate);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute.pipeline_layout, 0, {descriptor_set},
                                      nullptr);
    command_buffer.dispatch(compute.workgroup_num, 1, 1);


    /* queue family ownership transfer: release */
    if (need_queue_transfer)
    {
        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                       vk::PipelineStageFlagBits::eVertexShader, {}, {},
                                       {vk::BufferMemoryBarrier{
                                               .srcAccessMask       = vk::AccessFlagBits::eShaderWrite,
                                               .srcQueueFamilyIndex = _device->queue_compute().family_index,
                                               .dstQueueFamilyIndex = _device->queue().family_index,
                                               .buffer              = storage_buffer->vkbuffer(),
                                               .offset              = 0,
                                               .size                = storage_buffer->buffer_size(),
                                       }},
                                       {});
    }
    command_buffer.end();
}


void ComputeShaderNBody::particles_prepare()
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


void ComputeShaderNBody::storage_buffer_prepare()
{
    assert(!particles.empty());
    spdlog::debug("[NBody] storage buffer prepare");


    /* create storage buffer */
    vk::DeviceSize storage_buffer_size = particles.size() * sizeof(Particle);

    storage_buffer = new Hiss::Buffer(*_device, storage_buffer_size,
                                      vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
                                              | vk::BufferUsageFlagBits::eStorageBuffer,
                                      vk::MemoryPropertyFlagBits::eDeviceLocal);
    auto stage_buffer =
            Hiss::Buffer(*_device, storage_buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    stage_buffer.memory_copy_in(particles.data(), storage_buffer_size);


    /* stage buffer -> storage buffer */
    Hiss::OneTimeCommand command_buffer(*_device, _device->command_pool_compute());
    command_buffer().copyBuffer(stage_buffer.vkbuffer(), storage_buffer->vkbuffer(),
                                {vk::BufferCopy{.size = storage_buffer_size}});


    /* 进行一次 release，为了和 graphics pass 的 acqurie 匹配 */
    if (!Hiss::Queue::is_same_queue_family(_device->queue(), _device->queue_compute()))
    {
        command_buffer().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe,
                                         {}, {},
                                         {vk::BufferMemoryBarrier{
                                                 .srcAccessMask       = vk::AccessFlagBits::eTransferWrite,
                                                 .srcQueueFamilyIndex = _device->queue_compute().family_index,
                                                 .dstQueueFamilyIndex = _device->queue().family_index,
                                                 .buffer              = storage_buffer->vkbuffer(),
                                                 .offset              = 0,
                                                 .size                = storage_buffer_size,
                                         }},
                                         {});
    }


    command_buffer.exec();
}


void ComputeShaderNBody::graphics_uniform_buffer_prepare()
{
    graphics.ubo.projection = glm::perspective(glm::radians(60.f),
                                               static_cast<float>(_swapchain->get_extent().width)
                                                       / static_cast<float>(_swapchain->get_extent().height),
                                               0.1f, 256.f);
    graphics.ubo.projection[1][1] *= -1.f;
    graphics.ubo.view       = glm::lookAt(glm::vec3(8.f, 8.f, 8.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    graphics.ubo.screen_dim = glm::vec2(static_cast<float>(_swapchain->get_extent().width),
                                        static_cast<float>(_swapchain->get_extent().height));


    for (auto& uniform_buffer: graphics.uniform_buffers)
    {
        uniform_buffer =
                new Hiss::Buffer(*_device, sizeof(graphics.ubo), vk::BufferUsageFlagBits::eUniformBuffer,
                                 vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
        uniform_buffer->memory_copy_in(&graphics.ubo, sizeof(graphics.ubo));
    }
}


void ComputeShaderNBody::compute_uniform_buffer_prepare()
{
    assert(num_particles);

    /* init ubo */
    compute.ubo.particle_count = static_cast<int32_t>(num_particles);
    compute.ubo.delta_time     = 0.f;


    /* init uniform buffers */
    for (auto& uniform_buffer: compute.uniform_buffers)
    {
        uniform_buffer =
                new Hiss::Buffer(*_device, sizeof(compute.ubo), vk::BufferUsageFlagBits::eUniformBuffer,
                                 vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
        uniform_buffer->memory_copy_in(&compute.ubo, sizeof(compute.ubo));
    }
}


void ComputeShaderNBody::graphics_pipeline_prepare()
{
    /* shader stage */
    graphics.pipeline_state.shader_stage_add(
            _shader_loader->load(graphics.vert_shader_path, vk::ShaderStageFlagBits::eVertex));
    graphics.pipeline_state.shader_stage_add(
            _shader_loader->load(graphics.frag_shader_path, vk::ShaderStageFlagBits::eFragment));

    /* vertex, index and assembly */
    graphics.pipeline_state.vertex_input_binding_set({vk::VertexInputBindingDescription{
            .binding   = 0,
            .stride    = sizeof(Particle),
            .inputRate = vk::VertexInputRate::eVertex,
    }});
    graphics.pipeline_state.vertex_input_attribute_set({
            vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, pos)},
            vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, vel)},
    });
    graphics.pipeline_state.input_assemly_set(vk::PrimitiveTopology::ePointList);


    /* additive blend */
    graphics.pipeline_state.color_blend_attachment_set(vk::PipelineColorBlendAttachmentState{
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eOne,
            .colorBlendOp        = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstAlphaBlendFactor = vk::BlendFactor::eDstAlpha,
            .alphaBlendOp        = vk::BlendOp::eAdd,
            .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    });


    /* depth test */
    graphics.pipeline_state.depth_set(vk::PipelineDepthStencilStateCreateInfo{
            .depthTestEnable  = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp   = vk::CompareOp::eAlways,
            .back             = {.compareOp = vk::CompareOp::eAlways},
    });

    /* pipeline layout */
    graphics.pipeline_layout = _device->vkdevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = 1,
            .pSetLayouts    = &graphics.descriptor_set_layout,
    });
    graphics.pipeline_state.pipeline_layout_set(graphics.pipeline_layout);


    /* dynamic state */
    graphics.pipeline_state.dynamic_state_add(vk::DynamicState::eViewport);
    graphics.pipeline_state.dynamic_state_add(vk::DynamicState::eScissor);


    graphics.pipeline = graphics.pipeline_state.generate(*_device);
}


void ComputeShaderNBody::graphics_descriptor_set_prepare()
{
    /* descriptor set layout */
    graphics.descriptor_set_layout = _device->vkdevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(graphics_descriptor_bindings.size()),
            .pBindings    = graphics_descriptor_bindings.data(),
    });


    /* 创建 descriptor sets */
    assert(graphics.descriptor_set_layout);
    std::vector<vk::DescriptorSetLayout> set_layouts(IN_FLIGHT_CNT, graphics.descriptor_set_layout);
    graphics.descriptor_sets = _device->vkdevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool     = descriptor_pool,
            .descriptorSetCount = static_cast<uint32_t>(set_layouts.size()),
            .pSetLayouts        = set_layouts.data(),
    });


    /* 将 descriptor set 与 buffer，image 绑定起来 */
    vk::DescriptorImageInfo particle_image_descriptor = {
            .sampler     = graphics.tex_particle->sampler(),
            .imageView   = graphics.tex_particle->image_view(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    vk::DescriptorImageInfo gradient_image_descriptor = {
            .sampler     = graphics.tex_gradient->sampler(),
            .imageView   = graphics.tex_gradient->image_view(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    for (size_t i = 0; i < graphics.descriptor_sets.size(); ++i)
    {
        vk::DescriptorBufferInfo buffer_descriptor = {graphics.uniform_buffers[i]->vkbuffer(), 0, VK_WHOLE_SIZE};

        _device->vkdevice().updateDescriptorSets(
                {
                        vk::WriteDescriptorSet{.dstSet          = graphics.descriptor_sets[i],
                                               .dstBinding      = 0,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                                               .pImageInfo      = &particle_image_descriptor},
                        vk::WriteDescriptorSet{.dstSet          = graphics.descriptor_sets[i],
                                               .dstBinding      = 1,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                                               .pImageInfo      = &gradient_image_descriptor},
                        vk::WriteDescriptorSet{.dstSet          = graphics.descriptor_sets[i],
                                               .dstBinding      = 2,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType  = vk::DescriptorType::eUniformBuffer,
                                               .pBufferInfo     = &buffer_descriptor},
                },
                {});
    }
}


void ComputeShaderNBody::graphics_load_assets()
{
    graphics.tex_particle = new Hiss::Texture(*_device, graphics.tex_particle_path, false);
    graphics.tex_gradient = new Hiss::Texture(*_device, graphics.tex_gradient_path, false);
}


void ComputeShaderNBody::graphcis_command_record(vk::CommandBuffer command_buffer, vk::Framebuffer framebuffer,
                                                 vk::DescriptorSet descriptor_set, vk::Image color_image)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});

    const std::array<vk::ClearValue, 2> clear_values = {
            vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.f}}},
            vk::ClearValue{.depthStencil = {1.f, 0}},
    };


    bool need_transfer_compute =
            !Hiss::Queue::is_same_queue_family(_device->queue(), _device->queue_compute());

    /* execution barrier(depth attachment) */
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eLateFragmentTests,
                                   vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead
                                                          | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                                           .oldLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                           .newLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                           .image            = _depth_image->vkimage(),
                                           .subresourceRange = _depth_image_view->subresource_range(),
                                   }});

    /* layout transition(color attachment): present -> color attach；不需要保持数据，不需要 queue transfer */
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .dstAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite,
                                           .oldLayout        = vk::ImageLayout::eUndefined,
                                           .newLayout        = vk::ImageLayout::eColorAttachmentOptimal,
                                           .image            = color_image,
                                           .subresourceRange = _swapchain->get_image_subresource_range(),
                                   }});

    /* queue ownership transfer acquire(storage buffer): compute -> graphics */
    if (need_transfer_compute)
    {
        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                       vk::PipelineStageFlagBits::eVertexInput, {}, {},
                                       {vk::BufferMemoryBarrier{
                                               .dstAccessMask       = vk::AccessFlagBits::eVertexAttributeRead,
                                               .srcQueueFamilyIndex = _device->queue_compute().family_index,
                                               .dstQueueFamilyIndex = _device->queue().family_index,
                                               .buffer              = storage_buffer->vkbuffer(),
                                               .offset              = 0,
                                               .size                = storage_buffer->buffer_size(),
                                       }},
                                       {});
    }


    /* render pass */
    command_buffer.beginRenderPass(
            vk::RenderPassBeginInfo{
                    .renderPass      = _simple_render_pass,
                    .framebuffer     = framebuffer,
                    .renderArea      = {.offset = {0, 0}, .extent = _swapchain->get_extent()},
                    .clearValueCount = static_cast<uint32_t>(clear_values.size()),
                    .pClearValues    = clear_values.data(),
            },
            vk::SubpassContents::eInline);
    command_buffer.setViewport(0, {vk::Viewport{0.f, 0.f, (float) _swapchain->get_extent().width,
                                                (float) _swapchain->get_extent().height, 0.f, 1.f}});
    command_buffer.setScissor(0, {vk::Rect2D{.offset = {0, 0}, .extent = _swapchain->get_extent()}});
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics.pipeline);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics.pipeline_layout, 0, {descriptor_set},
                                      {});
    command_buffer.bindVertexBuffers(0, {storage_buffer->vkbuffer()}, {0});
    command_buffer.draw(num_particles, 1, 0, 0);
    command_buffer.endRenderPass();


    /* queue release(storage buffer): graphics -> compute */
    if (need_transfer_compute)
    {
        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eVertexInput,
                                       vk::PipelineStageFlagBits::eComputeShader, {}, {},
                                       {vk::BufferMemoryBarrier{
                                               .srcAccessMask       = vk::AccessFlagBits::eVertexAttributeRead,
                                               .srcQueueFamilyIndex = _device->queue().family_index,
                                               .dstQueueFamilyIndex = _device->queue_compute().family_index,
                                               .buffer              = storage_buffer->vkbuffer(),
                                               .offset              = 0,
                                               .size                = storage_buffer->buffer_size(),
                                       }},
                                       {});
    }

    /* queue release(color attach): graphics -> present; layout transition: color -> present */
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {},
                                   {vk::ImageMemoryBarrier{
                                           .srcAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
                                           .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
                                           .newLayout           = vk::ImageLayout::ePresentSrcKHR,
                                           .srcQueueFamilyIndex = _device->queue().family_index,
                                           .dstQueueFamilyIndex = _device->queue_present().family_index,
                                           .image               = color_image,
                                           .subresourceRange    = _swapchain->get_image_subresource_range(),
                                   }});

    command_buffer.end();
}


void ComputeShaderNBody::update(double delta_time) noexcept
{
    delta_time /= 1000;
    Hiss::Engine::preupdate(delta_time);

    prepare_frame();


    /* update uniform buffers */
    compute_uniform_update(*compute.uniform_buffers[current_frame_index()], (float) delta_time);
    graphics_uniform_update(*graphics.uniform_buffers[current_frame_index()], (float) delta_time);


    /* record graphics command */
    vk::CommandBuffer graphics_command_buffer = current_frame().command_buffer_graphics();
    graphics_command_buffer.reset();
    graphcis_command_record(graphics_command_buffer, _framebuffers[current_swapchain_image_index()],
                            graphics.descriptor_sets[current_frame_index()],
                            _swapchain->get_image(current_swapchain_image_index()));


    /* graphics draw */
    std::array<vk::PipelineStageFlags, 2> graphics_wait_stages = {
            vk::PipelineStageFlagBits::eVertexInput,          // 此阶段等待 compute pipeline
            vk::PipelineStageFlagBits::eLateFragmentTests,    // 此阶段等待 swapchian 的 image
    };
    std::array<vk::Semaphore, 2> grahics_wait_semaphores = {
            compute.semaphores[current_frame_index()],
            current_frame().semaphore_swapchain_acquire(),
    };
    std::array<vk::Semaphore, 2> graphics_signal_semaphores = {
            graphics.semaphores[current_frame_index()],
            current_frame().semaphore_render_complete(),
    };
    vk::Fence graphics_fence = _device->fence_pool().acquire(false);
    _device->queue().queue.submit(
            vk::SubmitInfo{
                    .waitSemaphoreCount   = static_cast<uint32_t>(grahics_wait_semaphores.size()),
                    .pWaitSemaphores      = grahics_wait_semaphores.data(),
                    .pWaitDstStageMask    = graphics_wait_stages.data(),
                    .commandBufferCount   = 1,
                    .pCommandBuffers      = &graphics_command_buffer,
                    .signalSemaphoreCount = static_cast<uint32_t>(graphics_signal_semaphores.size()),
                    .pSignalSemaphores    = graphics_signal_semaphores.data(),
            },
            graphics_fence);
    current_frame().fence_push(graphics_fence);


    submit_frame();


    /* compute: 更新 storage buffer */
    vk::PipelineStageFlags compute_wait_stage        = vk::PipelineStageFlagBits::eComputeShader;
    vk::Semaphore          compute_wait_semaphore    = graphics.semaphores[current_frame_index()];
    vk::Semaphore          compute_singlal_semaphore = compute.semaphores[current_frame_index()];
    vk::Fence              compute_fence             = _device->fence_pool().acquire(false);
    vk::CommandBuffer      compute_command_buffer    = current_frame().command_buffer_compute();
    _device->queue_compute().queue.submit(
            vk::SubmitInfo{
                    .waitSemaphoreCount   = 1,
                    .pWaitSemaphores      = &compute_wait_semaphore,
                    .pWaitDstStageMask    = &compute_wait_stage,
                    .commandBufferCount   = 1,
                    .pCommandBuffers      = &compute_command_buffer,
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores    = &compute_singlal_semaphore,
            },
            compute_fence);
    current_frame().fence_push(compute_fence);
}


void ComputeShaderNBody::compute_uniform_update(Hiss::Buffer& buffer, float delta_time)
{
    compute.ubo.delta_time = delta_time;

    buffer.memory_copy_in(&compute.ubo, sizeof(compute.ubo));
}


void ComputeShaderNBody::compute_clean()
{
    spdlog::info("[NBody] compute clean");

    for (auto& semaphore: compute.semaphores)
    {
        _device->vkdevice().destroy(semaphore);
    }
    _device->vkdevice().destroy(compute.descriptor_set_layout);
    _device->vkdevice().destroy(compute.pipeline_layout);
    _device->vkdevice().destroy(compute.pipeline_intergrate);
    _device->vkdevice().destroy(compute.pipeline_calculate);
    for (auto& uniform_buffer: compute.uniform_buffers)
    {
        DELETE(uniform_buffer);
    }
}


void ComputeShaderNBody::graphics_clean()
{
    spdlog::info("[NBody] graphics clean");

    DELETE(graphics.tex_particle);
    DELETE(graphics.tex_gradient);

    for (auto& semaphore: graphics.semaphores)
    {
        _device->vkdevice().destroy(semaphore);
    }

    _device->vkdevice().destroy(graphics.descriptor_set_layout);
    _device->vkdevice().destroy(graphics.pipeline_layout);
    _device->vkdevice().destroy(graphics.pipeline);
    for (auto& uniform_buffer: graphics.uniform_buffers)
    {
        DELETE(uniform_buffer);
    }
}


void ComputeShaderNBody::graphics_uniform_update(Hiss::Buffer& buffer, float delta_time)
{
    // IMPL，最好是检测到 window event 时更新
    // IMPL window 大小改变时，需要改变 ubo 的 screen dim
}

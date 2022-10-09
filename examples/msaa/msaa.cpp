#include "msaa.hpp"
#include "run.hpp"


APP_RUN(MSAA)


void MSAA::App::prepare()
{
    Engine::prepare();
    spdlog::info("[MSAA] prepare");


    _sample = _physical_device->max_sample_cnt();
    spdlog::info("[MSAA] max sample count: {}", vk::to_string(_sample));


    /* assets */
    _mesh    = new Hiss::Mesh(*_device, model_path);
    _texture = new Hiss::Texture(*_device, nullptr, texture_path, true, vk::Format::eR16G16B16A16Uint);


    /* 组件 */
    render_pass_prepare();
    descriptor_set_layout_prepare();
    pipeline_prepare();
    msaa_framebuffer_prepare();
    uniform_buffer_prepare();
    descriptor_set_prepare();
}


void MSAA::App::clean()
{
    spdlog::info("[MSAA] clean");

    /* assets */
    DELETE(_mesh);
    DELETE(_texture);


    /* 关键组件 */
    descriptor_clean();
    uniform_buffer_clean();
    msaa_framebuffer_clean();
    pipeline_clean();
    render_pass_clean();


    Engine::clean();
}


void MSAA::App::resize()
{
    Engine::resize();
    spdlog::info("[MSAA] on_resize");


    msaa_framebuffer_clean();
    msaa_framebuffer_prepare();
}


void MSAA::App::msaa_framebuffer_prepare()
{
    spdlog::info("[MSAA] create framebuffers");


    /* color image and image view */
    _msaa_color_image = new Hiss::Image(Hiss::Image::CreateInfo{
            .device            = *_device,
            .format            = _swapchain->color_format(),
            .extent            = _swapchain->get_extent(),
            .usage             = vk::ImageUsageFlagBits::eColorAttachment,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            .samples           = _sample,
    });
    _msaa_color_image->layout_tran(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                                   vk::ImageAspectFlagBits::eColor, 0, 1);
    _msaa_color_view = new Hiss::ImageView(*_msaa_color_image, vk::ImageAspectFlagBits::eColor, 0, 1);


    /* depth image and image view */
    _msaa_depth_image = new Hiss::Image(Hiss::Image::CreateInfo{
            .device            = *_device,
            .format            = get_depth_format(),
            .extent            = _swapchain->get_extent(),
            .usage             = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            .samples           = _sample,
    });
    _msaa_depth_image->layout_tran(vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                   vk::ImageAspectFlagBits::eDepth, 0, 1);
    _msaa_depth_view = new Hiss::ImageView(*_msaa_depth_image, vk::ImageAspectFlagBits::eDepth, 0, 1);


    /* framebuffer */
    _msaa_framebuffers.resize(_swapchain->image_number());
    assert(_msaa_renderpass);
    for (size_t i = 0; i < _msaa_framebuffers.size(); ++i)
    {
        _msaa_framebuffers[i] = new Hiss::Framebuffer3(*_device, _msaa_renderpass, _swapchain->get_extent(),
                                                       _msaa_color_view->view_get(), _msaa_depth_view->view_get(),
                                                       _swapchain->get_image_view(i));
    }
}


void MSAA::App::msaa_framebuffer_clean()
{
    spdlog::info("[MSAA] framebuffers clean");

    for (auto& framebuffer: _msaa_framebuffers)
        DELETE(framebuffer);

    DELETE(_msaa_color_view);
    DELETE(_msaa_color_image);
    DELETE(_msaa_depth_view);
    DELETE(_msaa_depth_image);
}


void MSAA::App::render_pass_prepare()
{
    spdlog::info("[MSAA] render pass prepare");


    /* attachments */
    std::vector<vk::AttachmentDescription> attachments = {
            /* color attachment */
            vk::AttachmentDescription{
                    .format        = _swapchain->color_format(),
                    .samples       = _sample,
                    .loadOp        = vk::AttachmentLoadOp::eClear,
                    .storeOp       = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .finalLayout   = vk::ImageLayout::eColorAttachmentOptimal,
            },
            /* depth attachment */
            vk::AttachmentDescription{
                    .format        = _depth_format,
                    .samples       = _sample,
                    .loadOp        = vk::AttachmentLoadOp::eClear,
                    .storeOp       = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    .finalLayout   = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            },
            /* resolve attachment */
            vk::AttachmentDescription{
                    .format        = _swapchain->color_format(),
                    .samples       = vk::SampleCountFlagBits::e1,
                    .loadOp        = vk::AttachmentLoadOp::eDontCare,
                    .storeOp       = vk::AttachmentStoreOp::eStore,
                    .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .finalLayout   = vk::ImageLayout::eColorAttachmentOptimal,
            },
    };


    /* subpass and attachment reference */
    vk::AttachmentReference color_ref = {
            .attachment = 0,
            .layout     = vk::ImageLayout::eColorAttachmentOptimal,
    };
    vk::AttachmentReference depth_ref = {
            .attachment = 1,
            .layout     = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };
    vk::AttachmentReference resolve_ref = {
            .attachment = 2,
            .layout     = vk::ImageLayout::eColorAttachmentOptimal,
    };
    vk::SubpassDescription subpass = {
            .pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount    = 1,
            .pColorAttachments       = &color_ref,
            .pResolveAttachments     = &resolve_ref,
            .pDepthStencilAttachment = &depth_ref,
    };


    /* render pass */
    _msaa_renderpass = _device->vkdevice().createRenderPass(vk::RenderPassCreateInfo{
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments    = attachments.data(),
            .subpassCount    = 1,
            .pSubpasses      = &subpass,
            .dependencyCount = 0,    // 外部使用 pipeline barrier 进行
            .pDependencies   = nullptr,
    });
}


void MSAA::App::render_pass_clean()
{
    spdlog::info("[MSAA] render pass clean");
    _device->vkdevice().destroy(_msaa_renderpass);
}


void MSAA::App::pipeline_clean()
{
    spdlog::info("[MSAA] pipeline clean");
    _device->vkdevice().destroy(_pipeline_layout);
    _device->vkdevice().destroy(_pipeline);
}


void MSAA::App::pipeline_prepare()
{
    spdlog::info("[MSAA] pipeline create");

    _pipeline_state.shader_stage_add(_shader_loader->load(vert_shader_path, vk::ShaderStageFlagBits::eVertex));
    _pipeline_state.shader_stage_add(_shader_loader->load(frag_shader_path, vk::ShaderStageFlagBits::eFragment));

    _pipeline_state.vertex_input_binding_set(Hiss::Vertex3DColorUv::binding_description_get(0));
    _pipeline_state.vertex_input_attribute_set(Hiss::Vertex3DColorUv::attribute_description_get(0));

    _pipeline_state.set_msaa(_sample);

    /* 在 view 使用 dynamic state，这样不用 recreate */
    _pipeline_state.dynamic_state_add(vk::DynamicState::eViewport);
    _pipeline_state.dynamic_state_add(vk::DynamicState::eScissor);

    /* pipeline descriptor */
    assert(_descriptor_set_layout);
    _pipeline_layout = _device->vkdevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
            .setLayoutCount = 1,
            .pSetLayouts    = &_descriptor_set_layout,
    });
    _pipeline_state.pipeline_layout_set(_pipeline_layout);

    assert(_msaa_renderpass);
    _pipeline = _pipeline_state.generate(*_device);
}


void MSAA::App::update(double delte_time) noexcept
{
    Hiss::Engine::preupdate(delte_time);
    prepare_frame();


    /* record command */
    uniform_update();
    vk::CommandBuffer command_buffer = current_frame().command_buffer_graphics();
    this->command_record(command_buffer, current_swapchain_image_index());


    /* submit command */
    vk::Fence                             fence             = _device->fence_pool().acquire(false);
    std::array<vk::PipelineStageFlags, 1> wait_stages       = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    std::array<vk::Semaphore, 1>          wait_semaphores   = {current_frame().semaphore_swapchain_acquire()};
    std::array<vk::Semaphore, 1>          signal_semaphores = {current_frame().semaphore_render_complete()};
    _device->queue().queue.submit_commands(
            {vk::SubmitInfo{
                    .waitSemaphoreCount   = static_cast<uint32_t>(wait_semaphores.size()),
                    .pWaitSemaphores      = wait_semaphores.data(),
                    .pWaitDstStageMask    = wait_stages.data(),
                    .commandBufferCount   = 1,
                    .pCommandBuffers      = &command_buffer,
                    .signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size()),
                    .pSignalSemaphores    = signal_semaphores.data(),
            }},
            fence);
    current_frame().fence_push(fence);


    submit_frame();
}


void MSAA::App::descriptor_set_layout_prepare()
{
    spdlog::info("[MSAA] descriptor set layout prepare");

    /* descriptor set layout */
    std::array<vk::DescriptorSetLayoutBinding, 2> descriptor_bindings = {
            /* MVP matrix UBO */
            vk::DescriptorSetLayoutBinding{
                    .binding         = 0,
                    .descriptorType  = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = 1,
                    .stageFlags      = vk::ShaderStageFlagBits::eVertex,
            },
            /* texture sampler */
            vk::DescriptorSetLayoutBinding{
                    .binding            = 1,
                    .descriptorType     = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount    = 1,
                    .stageFlags         = vk::ShaderStageFlagBits::eFragment,
                    .pImmutableSamplers = nullptr,
            },
    };
    _descriptor_set_layout = _device->vkdevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(descriptor_bindings.size()),
            .pBindings    = descriptor_bindings.data(),
    });
}


/**
 * descriptor set layout, descriptor set, uniform buffer, texture sampler
 */
void MSAA::App::descriptor_set_prepare()
{
    spdlog::info("[MSAA] descriptor set prepare");


    /* descriptor pool create */
    std::vector<vk::DescriptorPoolSize> pool_size_info = {
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = IN_FLIGHT_CNT},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = IN_FLIGHT_CNT},
    };
    _descriptor_pool = _device->vkdevice().createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .maxSets       = IN_FLIGHT_CNT,
            .poolSizeCount = static_cast<uint32_t>(pool_size_info.size()),
            .pPoolSizes    = pool_size_info.data(),
    });


    /* descriptor set create */
    std::vector<vk::DescriptorSetLayout> layouts(IN_FLIGHT_CNT, _descriptor_set_layout);
    _descriptor_sets = _device->vkdevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool     = _descriptor_pool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts        = layouts.data(),
    });


    /* bind uniform buffer and descriptor set */
    for (size_t i = 0; i < IN_FLIGHT_CNT; ++i)
    {
        /* binding 0: uniform buffer */
        vk::DescriptorBufferInfo buffer_info = {
                .buffer = _uniform_buffers[i]->vkbuffer(),
                .offset = 0,
                .range  = _uniform_buffers[i]->buffer_size(),
        };
        vk::WriteDescriptorSet buffer_write = {
                .dstSet          = _descriptor_sets[i],
                .dstBinding      = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo     = &buffer_info,
        };

        /* binding 1: texture sampler */
        assert(_texture);
        vk::DescriptorImageInfo image_info = {
                .sampler     = _texture->sampler(),
                .imageView   = _texture->image_view(),
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        vk::WriteDescriptorSet image_write = {
                .dstSet          = _descriptor_sets[i],
                .dstBinding      = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo      = &image_info,
        };

        _device->vkdevice().updateDescriptorSets({buffer_write, image_write}, {});
    }
}


void MSAA::App::descriptor_clean()
{
    spdlog::info("[MSAA] descriptor set clear");

    _descriptor_sets.clear();    // 跟随 pool 一起销毁

    _device->vkdevice().destroy(_descriptor_set_layout);

    _device->vkdevice().destroy(_descriptor_pool);
}


void MSAA::App::uniform_buffer_prepare()
{
    spdlog::info("[MSAA] uniform buffer create");

    for (size_t i = 0; i < IN_FLIGHT_CNT; ++i)
    {
        _uniform_buffers[i] =
                new Hiss::Buffer(*_device, sizeof(UniformBlock), vk::BufferUsageFlagBits::eUniformBuffer,
                                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}


void MSAA::App::uniform_buffer_clean()
{
    spdlog::info("[MSAA] uniform buffer clear");

    for (auto& uniform_buffer: _uniform_buffers)
    {
        DELETE(uniform_buffer);
    }
}


void MSAA::App::command_record(vk::CommandBuffer command_buffer, uint32_t swapchain_image_index)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});

    /* color attachment: execution barrier */
    assert(_msaa_color_image);
    assert(_msaa_color_view);
    vk::ImageMemoryBarrier color_execution_barrier = {
            .dstAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout        = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout        = vk::ImageLayout::eColorAttachmentOptimal,
            .image            = _msaa_color_image->vkimage(),
            .subresourceRange = _msaa_color_view->subresource_range(),
    };
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                                   {color_execution_barrier});

    /* depth attachment: execution barrier */
    assert(_msaa_depth_image);
    assert(_msaa_depth_view);
    vk::ImageMemoryBarrier depth_execution_barrier = {
            .dstAccessMask =
                    vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            .oldLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .newLayout        = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .image            = _msaa_depth_image->vkimage(),
            .subresourceRange = _msaa_depth_view->subresource_range(),
    };
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eLateFragmentTests,
                                   vk::PipelineStageFlagBits::eEarlyFragmentTests, {}, {}, {},
                                   {depth_execution_barrier});

    /* resolve attachment: layout transition */
    vk::ImageMemoryBarrier resolve_layout_transition = {
            .dstAccessMask    = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout        = vk::ImageLayout::eUndefined,
            .newLayout        = vk::ImageLayout::eColorAttachmentOptimal,
            .image            = _swapchain->get_image(swapchain_image_index),
            .subresourceRange = _swapchain->get_image_subresource_range(),
    };
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                                   {resolve_layout_transition});    // src stage 有 semaphore 保证


    /* render pass */
    command_buffer.beginRenderPass(
            vk::RenderPassBeginInfo{
                    .renderPass      = _msaa_renderpass,
                    .framebuffer     = _msaa_framebuffers[swapchain_image_index]->framebuffer(),
                    .renderArea      = {.offset = {0, 0}, .extent = _swapchain->get_extent()},
                    .clearValueCount = static_cast<uint32_t>(clear_values.size()),
                    .pClearValues    = clear_values.data(),
            },
            vk::SubpassContents::eInline);
    assert(_mesh);
    command_buffer.bindVertexBuffers(0, {_mesh->vertex_buffer().vkbuffer()}, {0});
    command_buffer.bindIndexBuffer(_mesh->index_buffer().vkbuffer(), 0, vk::IndexType::eUint32);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout, 0,
                                      {_descriptor_sets[current_frame_index()]}, {});
    viewport.width  = static_cast<float>(_swapchain->get_extent().width);
    viewport.height = static_cast<float>(_swapchain->get_extent().height);
    command_buffer.setViewport(0, {viewport});
    command_buffer.setScissor(0, {vk::Rect2D{.offset = {0, 0}, .extent = _swapchain->get_extent()}});
    command_buffer.drawIndexed(static_cast<uint32_t>(_mesh->index_cnt()), 1, 0, 0, 0);
    command_buffer.endRenderPass();


    /* resolve attachment: release and layout transition */
    bool need_release = !Hiss::Queue::is_same_queue_family(_device->queue_present(), _device->queue());
    vk::ImageMemoryBarrier release_barrier = {
            .srcAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite,
            .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout           = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = need_release ? _device->queue().family_index : VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = need_release ? _device->queue_present().family_index : VK_QUEUE_FAMILY_IGNORED,
            .image               = _swapchain->get_image(swapchain_image_index),
            .subresourceRange    = _swapchain->get_image_subresource_range(),
    };
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, {release_barrier});


    command_buffer.end();
}


void MSAA::App::uniform_update()
{
    assert(_uniform_buffers[current_frame_index()] != nullptr);


    static auto start_time = std::chrono::high_resolution_clock::now();
    auto        cur_time   = std::chrono::high_resolution_clock::now();
    float       time       = std::chrono::duration<float, std::chrono::seconds::period>(cur_time - start_time).count();

    UniformBlock ubo = {
            .model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f)),
            .view  = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f)),
            .proj  = glm::perspective(glm::radians(45.f),
                                      static_cast<float>(_swapchain->get_extent().width)
                                              / static_cast<float>(_swapchain->get_extent().height),
                                      0.1f, 10.f),
    };
    /* glm 是为 OpenGL 设计的，Vulkan 的坐标系和 OpenGL 存在差异 */
    ubo.proj[1][1] *= -1.f;

    _uniform_buffers[current_frame_index()]->memory_copy_in(&ubo, sizeof(ubo));
}
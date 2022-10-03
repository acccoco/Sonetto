#include "hello_triangle.hpp"
#include "run.hpp"
#include "vk/vk_common.hpp"


RUN(Hello::App)


void Hello::App::prepare()
{
    spdlog::info("[app] prepare");


    create_uniform_buffer();
    init_descriptor_set();
    create_pipeline();

    // 顶点数据
    _index_buffer  = new Hiss::IndexBuffer2(engine.device(), engine.allocator, indices);
    _vertex_buffer = new Hiss::VertexBuffer2<Hiss::Vertex2DColor>(engine.device(), engine.allocator, vertices);

    // 深度缓冲
    _depth_image = engine.create_depth_image();


    // 初始化 per frame payload
    _payloads.resize(engine.frame_manager().frames().size());
    for (auto& _payload: _payloads)
    {
        FramePayload payload;
        payload.command_buffer = engine.device().command_pool().command_buffer_create(1).front();
        _payload               = payload;
    }
}


void Hello::App::create_pipeline()
{
    // 着色器
    _pipeline_template.shader_stages.push_back(
            engine.shader_loader().load(shader_vert_path2, vk::ShaderStageFlagBits::eVertex));
    _pipeline_template.shader_stages.push_back(
            engine.shader_loader().load(shader_frag_path2, vk::ShaderStageFlagBits::eFragment));

    // 顶点属性
    _pipeline_template.vertex_bindings   = Hiss::Vertex2DColor::get_binding_description(0);
    _pipeline_template.vertex_attributes = Hiss::Vertex2DColor::get_attribute_description(0);

    _pipeline_template.set_viewport(engine.extent());

    // pipeline layout
    assert(_descriptor_set_layout);
    _pipeline_layout                   = engine.vkdevice().createPipelineLayout({
                              .setLayoutCount = 1,
                              .pSetLayouts    = &_descriptor_set_layout,
    });
    _pipeline_template.pipeline_layout = _pipeline_layout;

    // framebuffer 相关
    _pipeline_template.depth_format  = engine.device().gpu().depth_stencil_format();
    _pipeline_template.color_formats = {engine.color_format()};

    _pipeline = _pipeline_template.generate(engine.device());
}


void Hello::App::record_command(vk::CommandBuffer command_buffer, const FramePayload& payload, const Hiss::Frame& frame)
{
    command_buffer.begin(vk::CommandBufferBeginInfo{});


    // depth attachment execution barrier: 确保对 depth buffer 的写入不会乱序
    _depth_image->execution_barrier(
            {vk::PipelineStageFlagBits::eLateFragmentTests},
            {vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::AccessFlagBits::eDepthStencilAttachmentWrite},
            command_buffer);


    // color attachment layout transfer: undefined -> present （无需保留之前的内容）
    frame.image().transfer_layout({},
                                  Hiss::StageAccess{vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::AccessFlagBits::eColorAttachmentWrite},
                                  command_buffer, vk::ImageLayout::eColorAttachmentOptimal, true);

    auto color_attach_info = vk::RenderingAttachmentInfo{
            .imageView   = frame.image().view().vkview,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.2f, 0.2f, 0.2f, 1.f}}},
    };
    auto depth_attach_info = vk::RenderingAttachmentInfo{
            .imageView   = _depth_image->view().vkview,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eDontCare,
            .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
    };

    /* 绘制过程 */
    command_buffer.beginRendering(vk::RenderingInfo{
            .renderArea           = {.offset = {0, 0}, .extent = engine.extent()},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &color_attach_info,
            .pDepthAttachment     = &depth_attach_info,
    });


    assert(_vertex_buffer && _index_buffer);
    command_buffer.bindVertexBuffers(0, {_vertex_buffer->vkbuffer()}, {0});
    command_buffer.bindIndexBuffer(_index_buffer->vkbuffer(), 0, vk::IndexType::eUint32);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout, 0, {_descriptor_set}, {});
    command_buffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    command_buffer.endRendering();


    /* color attachment layout transition: color -> present */
    frame.image().transfer_layout(Hiss::StageAccess{vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::AccessFlagBits::eColorAttachmentWrite},
                                  {}, command_buffer, vk::ImageLayout::ePresentSrcKHR);


    command_buffer.end();
}


void Hello::App::update() noexcept
{
    auto& frame   = engine.current_frame();
    auto& payload = _payloads[frame.frame_id()];

    /* draw */
    record_command(payload.command_buffer, payload, frame);

    vk::PipelineStageFlags wait_stage       = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::Semaphore          wait_semaphore   = frame.acquire_semaphore();
    vk::Semaphore          signal_semaphore = frame.submit_semaphore();

    engine.device().vkqueue().submit({vk::SubmitInfo{
                                             .waitSemaphoreCount   = 1,
                                             .pWaitSemaphores      = &wait_semaphore,
                                             .pWaitDstStageMask    = &wait_stage,
                                             .commandBufferCount   = 1,
                                             .pCommandBuffers      = &payload.command_buffer,
                                             .signalSemaphoreCount = 1,
                                             .pSignalSemaphores    = &signal_semaphore,
                                     }},
                                     frame.insert_fence());
}


void Hello::App::resize()
{
    // pipeline
    engine.device().vkdevice().destroy(_pipeline);
    _pipeline_template.set_viewport(engine.extent());
    _pipeline = _pipeline_template.generate(engine.device());

    // depth attachmetn
    delete _depth_image;
    _depth_image = engine.create_depth_image();

    // payload
}


void Hello::App::clean()
{
    spdlog::info("[app] clean");

    // 清理 payload
    for (auto& payload: _payloads)
        ;

    // 清理其他的
    delete _depth_image;
    delete _vertex_buffer;
    delete _index_buffer;
    delete _uniform_buffer;

    engine.device().vkdevice().destroy(_pipeline);
    engine.device().vkdevice().destroy(_pipeline_layout);
    engine.vkdevice().destroy(_descriptor_set_layout);
}


void Hello::App::init_descriptor_set()
{
    // 创建 descriptor set layout
    _descriptor_set_layout = engine.vkdevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(descriptor_bindings.size()),
            .pBindings    = descriptor_bindings.data(),
    });

    // 创建 descriptor set
    _descriptor_set = engine.vkdevice()
                              .allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                                      .descriptorPool     = engine.descriptor_pool(),
                                      .descriptorSetCount = 1,
                                      .pSetLayouts        = &_descriptor_set_layout,
                              })
                              .front();

    // 绑定 uniform buffer
    assert(_uniform_buffer);
    auto uniform_buffer_info = vk::DescriptorBufferInfo{_uniform_buffer->vkbuffer(), 0, VK_WHOLE_SIZE};
    engine.vkdevice().updateDescriptorSets({vk::WriteDescriptorSet{
                                                   .dstSet          = _descriptor_set,
                                                   .dstBinding      = 0,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType  = vk::DescriptorType::eUniformBuffer,
                                                   .pBufferInfo     = &uniform_buffer_info,
                                           }},
                                           {});
}


void Hello::App::create_uniform_buffer()
{
    _uniform_buffer = new Hiss::UniformBuffer(engine.allocator, sizeof(UniformData));
    _uniform_buffer->memory_copy(&_ubo, sizeof(UniformData));
}

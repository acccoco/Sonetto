#pragma once
#include <array>
#include "core/engine.hpp"
#include "func/pipeline_template.hpp"
#include "proj_config.hpp"
#include "func/model.hpp"
#include "func/texture.hpp"
#include "utils/tools.hpp"
#include "application.hpp"


namespace MSAA
{

struct UniformBlock
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};


struct Payload
{
    Hiss::UniformBuffer* uniform_buffer{};
    vk::DescriptorSet    descriptor_set;
    UniformBlock         ubo{};
    vk::CommandBuffer    command_buffer;
};


/**
 * MSAA 实现：
 * - framebuffer 需要有 3 个 attachment：color，depth，resolve
 * - color 和 depth attachment 都需要多个 sample
 */
class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {}
    ~App() override = default;


    std::vector<Payload> payloads;


private:
    void prepare() override;
    void update() noexcept override;
    void resize() override {}
    void clean() override;


    void prepare_pipeline();


    // 创建 descriptor set，与 buffer 绑定起来
    void prepare_descriptor_set();

    void command_record(vk::CommandBuffer command_buffer, const MSAA::Payload& payload, const Hiss::Frame& frame);
    void uniform_update(Hiss::UniformBuffer& uniform_buffer);


    // members =======================================================


public:
    const std::string vert_shader_path = shader / "msaa/msaa.vert";
    const std::string frag_shader_path = shader / "msaa/msaa.frag";

    const vk::SampleCountFlagBits msaa_sample = engine.device().gpu().max_msaa_cnt();


    Hiss::Mesh    mesh = Hiss::Mesh(engine, model / "viking_room/viking_room.obj");
    Hiss::Texture tex  = Hiss::Texture(engine.device(), engine.allocator, model / "viking_room/tex/viking_room.png",
                                       vk::Format::eR8G8B8A8Srgb);


#pragma region pipeline 相关
    const std::vector<vk::DescriptorSetLayoutBinding> descriptor_bindings = {
            vk::DescriptorSetLayoutBinding{
                    .binding         = 0,
                    .descriptorType  = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = 1,
                    .stageFlags      = vk::ShaderStageFlagBits::eVertex,
            },
            vk::DescriptorSetLayoutBinding{
                    .binding            = 1,
                    .descriptorType     = vk::DescriptorType::eCombinedImageSampler,
                    .descriptorCount    = 1,
                    .stageFlags         = vk::ShaderStageFlagBits::eFragment,
                    .pImmutableSamplers = nullptr,
            }};

    const vk::Viewport viewport = {.width    = (float) engine.extent().width,
                                   .height   = (float) engine.extent().height,
                                   .minDepth = 0.f,
                                   .maxDepth = 1.f};

    const vk::Rect2D scissor = {{0, 0}, engine.extent()};


    vk::Pipeline            pipeline;
    vk::PipelineLayout      pipeline_layout;
    vk::DescriptorSetLayout descriptor_layout;

#pragma endregion


#pragma region framebuffer 相关信息

    Hiss::Image2D color_attach = Hiss::Image2D(engine.allocator, engine.device(),
                                               Hiss::Image2D::Info{
                                                       .format       = engine.color_format(),
                                                       .extent       = engine.extent(),
                                                       .usage        = vk::ImageUsageFlagBits::eColorAttachment,
                                                       .samples      = msaa_sample,
                                                       .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                                       .aspect       = vk::ImageAspectFlagBits::eColor,
                                                       .init_layout  = vk::ImageLayout::eColorAttachmentOptimal,
                                               });

    Hiss::Image2D depth_attach = Hiss::Image2D(engine.allocator, engine.device(),
                                               Hiss::Image2D::Info{
                                                       .format       = engine.depth_format(),
                                                       .extent       = engine.extent(),
                                                       .usage        = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                                       .samples      = msaa_sample,
                                                       .memory_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                                       .aspect       = vk::ImageAspectFlagBits::eDepth,
                                                       .init_layout  = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                                               });


    vk::RenderingAttachmentInfo color_attach_info = {
            .imageView          = color_attach.vkview(),
            .imageLayout        = vk::ImageLayout::eColorAttachmentOptimal,
            .resolveMode        = vk::ResolveModeFlagBits::eAverage,
            .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp             = vk::AttachmentLoadOp::eClear,
            .storeOp            = vk::AttachmentStoreOp::eDontCare,
            .clearValue         = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.f}}},
    };

    vk::RenderingAttachmentInfo depth_attach_info = {
            .imageView   = depth_attach.vkview(),
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eDontCare,
            .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
    };

    vk::RenderingInfo render_info = {
            .renderArea           = {.offset = {0, 0}, .extent = engine.extent()},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &color_attach_info,
            .pDepthAttachment     = &depth_attach_info,
    };

#pragma endregion
};
}    // namespace MSAA

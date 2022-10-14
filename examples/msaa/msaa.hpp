#pragma once
#include <array>
#include "core/engine.hpp"
#include "proj_config.hpp"
#include "func/model.hpp"
#include "func/pipeline_template.hpp"
#include "func/texture.hpp"
#include "func/vk_func.hpp"
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

    void record_command(vk::CommandBuffer command_buffer, const MSAA::Payload& payload, const Hiss::Frame& frame);
    void update_uniform(Hiss::UniformBuffer& uniform_buffer);


    // members =======================================================


public:
    const std::string vert_shader_path = shader / "msaa/msaa.vert";
    const std::string frag_shader_path = shader / "msaa/msaa.frag";

    const vk::SampleCountFlagBits msaa_sample = engine.device().gpu().max_msaa_cnt();


    Hiss::Mesh    mesh = Hiss::Mesh(engine, model / "viking_room/viking_room.obj");
    Hiss::Texture tex  = Hiss::Texture(engine.device(), engine.allocator, model / "viking_room/tex/viking_room.png",
                                       vk::Format::eR8G8B8A8Srgb);


#pragma region pipeline 相关

    vk::Pipeline       pipeline;
    vk::PipelineLayout pipeline_layout;

    vk::DescriptorSetLayout descriptor_layout = Hiss::Initial::descriptor_set_layout(
            engine.vkdevice(),
            {
                    {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},             // 0
                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},    // 1
            });

#pragma endregion


#pragma region framebuffer 相关信息

    Hiss::Image2D* color_attach = engine.create_color_attach(msaa_sample);
    Hiss::Image2D* depth_attach = engine.create_depth_attach(msaa_sample);

    vk::RenderingAttachmentInfo color_attach_info = Hiss::Initial::color_resolve_attach(color_attach->vkview());
    vk::RenderingAttachmentInfo depth_attach_info = Hiss::Initial::depth_attach_info(depth_attach->vkview());

    vk::RenderingInfo render_info = Hiss::Initial::render_info(color_attach_info, depth_attach_info, engine.extent());

#pragma endregion
};
}    // namespace MSAA

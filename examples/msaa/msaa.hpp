#pragma once
#include <array>

#include "proj_config.hpp"
#include "engine/engine.hpp"
#include "engine/model.hpp"
#include "engine/model2.hpp"
#include "engine/texture.hpp"
#include "utils/tools.hpp"
#include "utils/vk_func.hpp"
#include "utils/application.hpp"
#include "utils/pipeline_template.hpp"


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
    std::shared_ptr<Hiss::Buffer> uniform_buffer;
    vk::DescriptorSet             descriptor_set;
    UniformBlock                  ubo{};
    vk::CommandBuffer             command_buffer;
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

    Hiss::MeshLoader mesh2{engine, model / "viking_room" / "viking_room.obj"};


    UniformBlock ubo = {
            .view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f)),
            .proj = Hiss::perspective(45.f, (float) engine.extent().width / (float) engine.extent().height, 0.1f, 10.f),
    };


private:
    void prepare() override;
    void update() noexcept override;
    void resize() override {}
    void clean() override;


    void prepare_pipeline();


    // 创建 descriptor set，与 buffer 绑定起来
    void prepare_descriptor_set();

    void record_command(vk::CommandBuffer command_buffer, const MSAA::Payload& payload, const Hiss::Frame& frame);


    // members =======================================================


public:
    const std::string vert_shader_path = shader / "msaa/msaa.vert";
    const std::string frag_shader_path = shader / "msaa/msaa.frag";

    const vk::SampleCountFlagBits msaa_sample = engine.device().gpu().max_msaa_cnt();


    // Hiss::Mesh    mesh = Hiss::Mesh(engine, model / "viking_room/viking_room.obj");
    // Hiss::Texture tex  = Hiss::Texture(engine.device(), engine.allocator, model / "viking_room/tex/viking_room.png",
    // vk::Format::eR8G8B8A8Srgb);

    // Hiss::Mesh mesh2_cube = Hiss::Mesh(engine, model / "cube/cube.obj");



    vk::Pipeline       pipeline;
    vk::PipelineLayout pipeline_layout;

    vk::DescriptorSetLayout descriptor_layout = Hiss::Initial::descriptor_set_layout(
            engine.vkdevice(),
            {
                    {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},             // 0
                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},    // 1
            });

    std::shared_ptr<Hiss::DescriptorLayout> material_layout = Hiss::Matt::get_material_descriptor(engine.device());


    Hiss::Image2D* color_attach = engine.create_color_attach(msaa_sample);
    Hiss::Image2D* depth_attach = engine.create_depth_attach(msaa_sample);

    vk::RenderingAttachmentInfo color_attach_info = Hiss::Initial::color_resolve_attach(color_attach->vkview());
    vk::RenderingAttachmentInfo depth_attach_info = Hiss::Initial::depth_attach_info(depth_attach->vkview());

    vk::RenderingInfo render_info = Hiss::Initial::render_info(color_attach_info, depth_attach_info, engine.extent());
};
}    // namespace MSAA

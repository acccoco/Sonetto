#pragma once
#include "core/engine.hpp"
#include "func/vertex.hpp"
#include "proj_config.hpp"
#include "func/pipeline_template.hpp"
#include "application.hpp"
#include "func/vertex_buffer.hpp"
#include "func/vk_func.hpp"


namespace Hello
{
struct FramePayload
{
    vk::CommandBuffer command_buffer = VK_NULL_HANDLE;
};


struct UniformData
{
    alignas(16) glm::vec3 color1 = {0.8f, 0.0f, 0.0f};
    alignas(16) glm::vec3 color2 = {0.0f, 0.5f, 0.0f};
    alignas(16) glm::vec3 color3 = {0.0f, 0.0f, 0.5f};
    float single                 = 0.1f;
    alignas(16) float colors[3]  = {0.25f, 0.0f, 0.0f};
    alignas(16) float colors1[3] = {0.25f, 0.0f, 0.0f};
    alignas(16) float colors2[3] = {0.5f, 0.3f, 0.0f};
};


class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {}
    ~App() override = default;


    // 顶点数据
    std::vector<Hiss::Vertex2DColor> vertices = {
            {{-0.5f, -0.5f}, {0.8f, 0.0f, 0.0f}},    //
            {{0.5f, 0.5f}, {0.0f, 0.5f, 0.0f}},      //
            {{0.5f, -0.5f}, {0.0f, 0.0f, 0.5f}},     //
    };

    // 模型的顶点索引
    std::vector<uint32_t> indices = {0, 1, 2};


#pragma region 特殊的接口
public:
    void prepare() final;
    void update() noexcept final;
    void resize() final;
    void clean() final;

#pragma endregion


#pragma region 准备数据的方法
private:
    void create_pipeline();


    void record_command(vk::CommandBuffer command_buffer, const FramePayload& payload, const Hiss::Frame& frame);

    // 初始化 descriptor set layout ，创建descriptor set，将 descriptor set 与 uniform buffer 绑定起来
    void init_descriptor_set();

    // 创建 UBO，并写入初始值
    void create_uniform_buffer();

#pragma endregion


#pragma region 应用的成员字段
private:
    Hiss::PipelineTemplate _pipeline_template;
    vk::Pipeline           _pipeline;

    // descriptor set 的布局详情
    vk::DescriptorSetLayout _descriptor_set_layout = Hiss::Initial::descriptor_set_layout(
            engine.vkdevice(), {{vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment}});

    vk::PipelineLayout _pipeline_layout;
    vk::DescriptorSet  _descriptor_set = engine.create_descriptor_set(_descriptor_set_layout);

    Hiss::IndexBuffer2* _index_buffer = new Hiss::IndexBuffer2(engine.device(), engine.allocator, indices);
    Hiss::VertexBuffer2<Hiss::Vertex2DColor>* _vertex_buffer =
            new Hiss::VertexBuffer2(engine.device(), engine.allocator, vertices);

    Hiss::Image2D* _depth_image = engine.create_depth_attach(vk::SampleCountFlagBits::e1);

    UniformData          _ubo;
    Hiss::UniformBuffer* _uniform_buffer{};

    std::vector<FramePayload> _payloads = {};
#pragma endregion


#pragma region 应用数据
private:
    const std::filesystem::path shader_vert_path2 = shader / "hello_triangle/hello_triangle.vert";
    const std::filesystem::path shader_frag_path2 = shader / "hello_triangle/hello_triangle.frag";


    vk::RenderingAttachmentInfo color_attach_info = Hiss::Initial::color_attach_info();
    vk::RenderingAttachmentInfo depth_attach_info = Hiss::Initial::depth_attach_info(_depth_image->vkview());

    vk::RenderingInfo render_info = Hiss::Initial::render_info(color_attach_info, depth_attach_info, engine.extent());
#pragma endregion
};
}    // namespace Hello

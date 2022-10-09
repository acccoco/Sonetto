#pragma once
#include "engine.hpp"
#include "vk/vertex.hpp"
#include "proj_config.hpp"
#include "vk/pipeline.hpp"
#include "application.hpp"


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
    Hiss::PipelineTemplate  _pipeline_template;
    vk::Pipeline            _pipeline;
    vk::PipelineLayout      _pipeline_layout;
    vk::DescriptorSetLayout _descriptor_set_layout;
    vk::DescriptorSet       _descriptor_set;

    Hiss::IndexBuffer2*                       _index_buffer{};
    Hiss::VertexBuffer2<Hiss::Vertex2DColor>* _vertex_buffer{};

    Hiss::Image2D* _depth_image{};

    UniformData          _ubo;
    Hiss::UniformBuffer* _uniform_buffer{};

    std::vector<FramePayload> _payloads = {};
#pragma endregion


#pragma region 应用数据
private:
    // 顶点数据
    std::vector<Hiss::Vertex2DColor> vertices = {
            {{-0.5f, -0.5f}, {0.8f, 0.0f, 0.0f}},    //
            {{0.5f, 0.5f}, {0.0f, 0.5f, 0.0f}},      //
            {{0.5f, -0.5f}, {0.0f, 0.0f, 0.5f}},     //
    };

    // 模型的顶点索引
    std::vector<uint32_t> indices = {0, 1, 2};

    const std::filesystem::path shader_vert_path2 = shader / "hello_triangle/hello_triangle.vert";
    const std::filesystem::path shader_frag_path2 = shader / "hello_triangle/hello_triangle.frag";


    // descriptor set 的布局详情
    const std::vector<vk::DescriptorSetLayoutBinding> descriptor_bindings = {{{
            0,
            vk::DescriptorType::eUniformBuffer,
            1,
            vk::ShaderStageFlagBits::eFragment,
    }}};

    vk::Viewport viewport{.minDepth = 0.f, .maxDepth = 1.f};

#pragma endregion
};
}    // namespace Hello

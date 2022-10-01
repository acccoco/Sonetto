#pragma once
#include "engine.hpp"
#include "vk/vertex.hpp"
#include "proj_config.hpp"
#include "vk/pipeline.hpp"


class HelloTriangle
{
public:
    explicit HelloTriangle(Hiss::Engine& engine)
        : engine(engine)
    {}


    void prepare();
    void update() noexcept;
    void resize();
    void clean();


private:
    struct FramePayload
    {
        vk::CommandBuffer command_buffer = VK_NULL_HANDLE;
    };


    void init_pipeline();
    void record_command(vk::CommandBuffer command_buffer, const FramePayload& payload, const Hiss::Frame& frame);


private:
    Hiss::Engine& engine;

    // 顶点数据
    std::vector<Hiss::Vertex2DColor> vertices = {
            {{-0.5f, 0.0f}, {0.8f, 0.0f, 0.0f}},    //
            {{0.5f, 0.5f}, {0.0f, 0.5f, 0.0f}},     //
            {{0.5f, 0.0f}, {0.0f, 0.0f, 0.5f}},     //
    };

    // 模型的顶点索引
    std::vector<uint32_t> indices = {0, 1, 2};

    const std::string shader_vert_path = SHADER("hello_triangle/hello_triangle.vert.spv");
    const std::string shader_frag_path = SHADER("hello_triangle/hello_triangle.frag.spv");
    const std::filesystem::path shader_vert_path2 = shader_dir / "hello_triangle/hello_triangle.vert.spv";
    const std::filesystem::path shader_frag_path2 = shader_dir / "hello_triangle/hello_triangle.frag.spv";


    Hiss::PipelineTemplate _pipeline_template;
    vk::Pipeline           _pipeline;
    vk::PipelineLayout     _pipeline_layout;

    Hiss::IndexBuffer2*                       _index_buffer{};
    Hiss::VertexBuffer2<Hiss::Vertex2DColor>* _vertex_buffer{};

    Hiss::Image2D* _depth_image{};


    std::vector<FramePayload> _payloads = {};
};
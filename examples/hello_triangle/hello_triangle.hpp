#pragma once
#include "application.hpp"
#include "vk/vertex.hpp"
#include "proj_profile.hpp"
#include "vk/pipeline.hpp"


class HelloTriangle
{
public:
    explicit HelloTriangle(Hiss::Application& app)
        : app(app)
    {}


    void prepare();
    void update() noexcept;
    void resize();
    void clean();


private:
    struct FramePayload
    {
        vk::CommandBuffer command_buffer = VK_NULL_HANDLE;

        vk::RenderingAttachmentInfo color_attach_info = {};
        vk::RenderingAttachmentInfo depth_attach_info = {};
    };


    void init_pipeline();
    void record_command(vk::CommandBuffer command_buffer, const FramePayload& payload, const Hiss::Frame& frame);


private:
    Hiss::Application& app;

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


    Hiss::GraphicsPipelineTemplate _pipeline_template;
    vk::Pipeline                   _pipeline;
    vk::PipelineLayout             _pipeline_layout;
    // std::shared_ptr<Hiss::VertexBuffer<Hiss::Vertex2DColor>> _vertex_buffer;
    // std::shared_ptr<Hiss::IndexBuffer>                       _index_buffer;

    Hiss::IndexBuffer2*                       _index_buffer2{};
    Hiss::VertexBuffer2<Hiss::Vertex2DColor>* _vertex_buffer2{};

    Hiss::Image*     depth_image      = nullptr;
    Hiss::ImageView* depth_image_view = nullptr;


    std::vector<FramePayload> _payloads = {};
};
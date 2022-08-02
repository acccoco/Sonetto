#pragma once
#include "example_base.hpp"
#include "vertex.hpp"
#include "proj_profile.hpp"
#include "pipeline.hpp"


class HelloTriangle : public Hiss::ExampleBase
{
public:
    HelloTriangle()
        : Hiss::ExampleBase("hello triangle")
    {}
    ~HelloTriangle() override = default;


private:
    void prepare() override;
    void update() noexcept override;
    void resize() override;
    void clean() override;

    void pipeline_prepare();
    void command_record(vk::CommandBuffer command_buffer, uint32_t swapchain_image_index);
    void draw();


    // members =======================================================


public:
    std::vector<Hiss::Vertex2DColor> vertices = {
            {{-0.5f, 0.0f}, {0.8f, 0.0f, 0.0f}},    //
            {{0.5f, 0.5f}, {0.0f, 0.5f, 0.0f}},     //
            {{0.5f, 0.0f}, {0.0f, 0.0f, 0.5f}},     //
    };

    std::vector<uint32_t> indices = {
            0, 1, 2
    };

    const std::string shader_vert_path = SHADER("hello_triangle/hello_triangle.vert.spv");
    const std::string shader_frag_path = SHADER("hello_triangle/hello_triangle.frag.spv");


private:
    Hiss::GraphicsPipelineState                         _pipeline_state;
    vk::Pipeline                                        _pipeline;
    vk::PipelineLayout                                  _pipeline_layout;
    std::shared_ptr<Hiss::VertexBuffer<Hiss::Vertex2DColor>> _vertex_buffer;
    std::shared_ptr<Hiss::IndexBuffer>                  _index_buffer;
};
#pragma once
#include "vk_application.hpp"
#include "vk/vertex.hpp"
#include "proj_profile.hpp"
#include "vk/pipeline.hpp"


class HelloTriangle : public Hiss::VkApplication
{
public:
    HelloTriangle()
        : Hiss::VkApplication("hello triangle")
    {}
    ~HelloTriangle() override = default;


    struct FramePayload
    {
        vk::CommandBuffer command_buffer = VK_NULL_HANDLE;
        vk::Framebuffer   framebuffer    = VK_NULL_HANDLE;

        vk::RenderingAttachmentInfo color_attach_info = {};
        vk::RenderingAttachmentInfo depth_attach_info = {};
    };


private:
    void prepare() override;
    void update(double delte_time) noexcept override;
    void resize() override;
    void clean() override;

    void init_pipeline();
    void record_command(vk::CommandBuffer command_buffer, const FramePayload& payload, const Hiss::Frame2& frame);


    // members =======================================================


public:
    std::vector<Hiss::Vertex2DColor> vertices = {
            {{-0.5f, 0.0f}, {0.8f, 0.0f, 0.0f}},    //
            {{0.5f, 0.5f}, {0.0f, 0.5f, 0.0f}},     //
            {{0.5f, 0.0f}, {0.0f, 0.0f, 0.5f}},     //
    };

    std::vector<uint32_t> indices = {0, 1, 2};

    const std::string shader_vert_path = SHADER("hello_triangle/hello_triangle.vert.spv");
    const std::string shader_frag_path = SHADER("hello_triangle/hello_triangle.frag.spv");


private:
    Hiss::GraphicsPipelineTemplate                           _pipeline_template;
    vk::Pipeline                                             _pipeline;
    vk::PipelineLayout                                       _pipeline_layout;
    std::shared_ptr<Hiss::VertexBuffer<Hiss::Vertex2DColor>> _vertex_buffer;
    std::shared_ptr<Hiss::IndexBuffer>                       _index_buffer;

    Hiss::Image*     depth_image_2      = nullptr;
    Hiss::ImageView* depth_image_view_2 = nullptr;


    std::vector<FramePayload> _payloads = {};
};
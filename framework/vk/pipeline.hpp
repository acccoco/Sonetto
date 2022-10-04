#pragma once
#include "vk/device.hpp"


namespace Hiss
{

class PipelineTemplate
{
public:
    // 根据 template 内的各种属性，生成 pipeline
    vk::Pipeline generate(const Device& device);


    // 同时设置 viewport 以及 scissor
    void set_viewport(const vk::Extent2D& extent)
    {
        _viewport.width  = static_cast<float>(extent.width);
        _viewport.height = static_cast<float>(extent.height);
        _scissor.extent  = extent;
    }


    void set_msaa(vk::SampleCountFlagBits samples)
    {
        assert(samples != vk::SampleCountFlagBits::e1);
        _msaa_state.rasterizationSamples = samples;
        _msaa_state.sampleShadingEnable  = VK_TRUE;
    }


public:
    std::vector<vk::PipelineShaderStageCreateInfo>   shader_stages     = {};
    std::vector<vk::VertexInputBindingDescription>   vertex_bindings   = {};
    std::vector<vk::VertexInputAttributeDescription> vertex_attributes = {};

    // dynamic rendering 需要的格式信息
    std::vector<vk::Format> color_attach_formats = {};
    vk::Format              depth_attach_format  = {};

    vk::PipelineLayout            pipeline_layout = {};
    std::vector<vk::DynamicState> dynamic_states  = {};


    vk::PipelineDepthStencilStateCreateInfo depth_stencil_state = {
            .depthTestEnable  = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp   = vk::CompareOp::eLess,
            /* 这个选项可以只显示某个深度范围的 fragment */
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = VK_FALSE,
    };


    vk::PipelineColorBlendAttachmentState color_blend_state = {
            .blendEnable         = VK_FALSE,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp        = vk::BlendOp::eAdd,

            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp        = vk::BlendOp::eAdd,

            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };


    // 图元装配阶段
    vk::PipelineInputAssemblyStateCreateInfo assembly_state = {
            .topology               = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE,
    };


private:
    vk::Viewport _viewport = {.x = 0.f, .y = 0.f, .minDepth = 0.f, .maxDepth = 1.f};
    vk::Rect2D   _scissor  = {.offset = {0, 0}};


    const vk::PipelineRasterizationStateCreateInfo _rasterization_state = {
            /* 超出深度范围的深度是被 clamp 还是被丢弃，需要 GPU feature */
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,

            /* 绘制线框/绘制顶点/绘制实心图形（除了 FILL 外，其他的都需要 GPU feature） */
            .polygonMode = vk::PolygonMode::eFill,

            /* 背面剔除，发生在 _viewport 变换之后 */
            .cullMode  = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,

            /* 基于 bias 或 fragment 的斜率来更改 depth（在 shadow map 中会用到） */
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.f,
            .depthBiasClamp          = 0.f,
            .depthBiasSlopeFactor    = 0.f,

            /* 如果线宽不为 1，需要 GPU feature */
            .lineWidth = 1.f,
    };

    // msaa 相关
    vk::PipelineMultisampleStateCreateInfo _msaa_state = {
            .rasterizationSamples  = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = .2f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
    };


    vk::PipelineColorBlendStateCreateInfo _blend_state = {
            /* logic operation 似乎是独立于 blend 的，如果启用了 logic，就不再使用 blend 了 */
            .logicOpEnable = VK_FALSE,
            .logicOp       = vk::LogicOp::eCopy,

            /* 需要为每个 color attachment 指定 blend state */
            .attachmentCount = 1,
            .pAttachments    = &color_blend_state,
            .blendConstants  = {{0.f, 0.f, 0.f, 0.f}},
    };
};

}    // namespace Hiss
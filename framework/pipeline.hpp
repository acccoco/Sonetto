#pragma once
#include "device.hpp"


namespace Hiss
{

class GraphicsPipelineState
{
public:
    vk::Pipeline generate(const Device& device, vk::RenderPass renderpass, uint32_t subpass);


    void shader_stage_add(const vk::PipelineShaderStageCreateInfo& shader_stage);
    void vertex_input_binding_set(const std::vector<vk::VertexInputBindingDescription>& bindings);
    template<size_t N>
    void vertex_input_binding_set(const std::array<vk::VertexInputBindingDescription, N>& bindings);
    void vertex_input_attribute_set(const std::vector<vk::VertexInputAttributeDescription>& attributes);
    template<size_t N>
    void vertex_input_attribute_set(const std::array<vk::VertexInputAttributeDescription, N>& attributes);
    void viewport_set(const vk::Extent2D& extent);
    void msaa_set(vk::SampleCountFlagBits samples);
    void dynamic_state_add(vk::DynamicState state);
    void pipeline_layout_set(vk::PipelineLayout layout);


private:
    std::vector<vk::PipelineShaderStageCreateInfo> _shader_stages = {};

    std::vector<vk::VertexInputBindingDescription> _vertex_bindind_descriptions = {};

    std::vector<vk::VertexInputAttributeDescription> _vertex_attribute_descriptions = {};

    vk::PipelineInputAssemblyStateCreateInfo _assembly_state = {
            .topology               = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE,
    };

    vk::Viewport _viewport = {.x = 0.f, .y = 0.f, .minDepth = 0.f, .maxDepth = 1.f};

    vk::Rect2D _scissor = {.offset = {0, 0}};

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

    vk::PipelineMultisampleStateCreateInfo _msaa_state = {
            .rasterizationSamples  = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = .2f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
    };

    vk::PipelineDepthStencilStateCreateInfo _depth_stencil_state = {
            .depthTestEnable  = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp   = vk::CompareOp::eLess,
            /* 这个选项可以只显示某个深度范围的 fragment */
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = VK_FALSE,
    };

    vk::PipelineColorBlendAttachmentState _color_blend_state = {
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

    vk::PipelineColorBlendStateCreateInfo _blend_state = {
            .logicOpEnable = VK_FALSE,
            .logicOp       = vk::LogicOp::eCopy,
            /* 需要为每个 color attachment 指定 blend state */
            .attachmentCount = 1,
            .pAttachments    = &_color_blend_state,
            .blendConstants  = {{0.f, 0.f, 0.f, 0.f}},
    };

    std::vector<vk::DynamicState> _dynamic_states = {};

    vk::PipelineLayout _pipeline_layout = {};
};


template<size_t N>
void GraphicsPipelineState::vertex_input_binding_set(const std::array<vk::VertexInputBindingDescription, N>& bindings)
{
    _vertex_bindind_descriptions.clear();
    _vertex_bindind_descriptions.reserve(N);
    for (const auto& binding: bindings)
        _vertex_bindind_descriptions.push_back(binding);
}


template<size_t N>
void GraphicsPipelineState::vertex_input_attribute_set(
        const std::array<vk::VertexInputAttributeDescription, N>& attributes)
{
    _vertex_attribute_descriptions.clear();
    _vertex_attribute_descriptions.reserve(N);
    for (const auto& attribute: attributes)
        _vertex_attribute_descriptions.push_back(attribute);
}


}    // namespace Hiss
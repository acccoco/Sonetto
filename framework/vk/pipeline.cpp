#include "vk/pipeline.hpp"


vk::Pipeline Hiss::PipelineTemplate::generate(const Device& device)
{
    vk::PipelineVertexInputStateCreateInfo vertex_input_state = {
            .vertexBindingDescriptionCount   = static_cast<uint32_t>(vertex_bindings.size()),
            .pVertexBindingDescriptions      = vertex_bindings.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size()),
            .pVertexAttributeDescriptions    = vertex_attributes.data(),
    };

    vk::PipelineViewportStateCreateInfo viewport_state = {
            .viewportCount = 1,
            .pViewports    = &_viewport,
            .scissorCount  = 1,
            .pScissors     = &_scissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state = {
            .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
            .pDynamicStates    = dynamic_states.data(),
    };

    // dynamic rendering 相关的信息
    vk::PipelineRenderingCreateInfo attach_info = {
            .colorAttachmentCount    = static_cast<uint32_t>(color_attach_formats.size()),
            .pColorAttachmentFormats = color_attach_formats.data(),
            .depthAttachmentFormat   = depth_attach_format,
    };

    vk::GraphicsPipelineCreateInfo pipeline_info = {
            // dynamic rendering
            .pNext = &attach_info,

            /* shader stages */
            .stageCount = static_cast<uint32_t>(shader_stages.size()),
            .pStages    = shader_stages.data(),

            /* vertex */
            .pVertexInputState   = &vertex_input_state,
            .pInputAssemblyState = &assembly_state,

            // tessellation：只有明确启用了 tse 着色器时，tessellation 的设置才会生效
            .pTessellationState = &tessellation_state,

            /* pipeline 的固定功能 */
            .pViewportState      = &viewport_state,
            .pRasterizationState = &_rasterization_state,
            .pMultisampleState   = &_msaa_state,
            .pDepthStencilState  = &depth_stencil_state,
            .pColorBlendState    = &_blend_state,
            .pDynamicState       = &dynamic_state,

            .layout = pipeline_layout,

            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex  = -1,
    };

    auto [result, pipeline] = device.vkdevice().createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
    vk::resultCheck(result, fmt::format("fail to create pipeline: {}", vk::to_string(result)).c_str());
    return pipeline;
}

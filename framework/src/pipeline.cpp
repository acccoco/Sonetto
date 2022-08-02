#include "pipeline.hpp"


vk::Pipeline Hiss::GraphicsPipelineState::generate(const Device& device, vk::RenderPass renderpass, uint32_t subpass)
{
    vk::PipelineVertexInputStateCreateInfo vertex_input_state = {
            .vertexBindingDescriptionCount   = static_cast<uint32_t>(_vertex_bindind_descriptions.size()),
            .pVertexBindingDescriptions      = _vertex_bindind_descriptions.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(_vertex_attribute_descriptions.size()),
            .pVertexAttributeDescriptions    = _vertex_attribute_descriptions.data(),
    };

    vk::PipelineViewportStateCreateInfo viewport_state = {
            .viewportCount = 1,
            .pViewports    = &_viewport,
            .scissorCount  = 1,
            .pScissors     = &_scissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state = {
            .dynamicStateCount = static_cast<uint32_t>(_dynamic_states.size()),
            .pDynamicStates    = _dynamic_states.data(),
    };

    vk::GraphicsPipelineCreateInfo pipeline_info = {
            /* shader stages */
            .stageCount = static_cast<uint32_t>(_shader_stages.size()),
            .pStages    = _shader_stages.data(),

            /* vertex */
            .pVertexInputState   = &vertex_input_state,
            .pInputAssemblyState = &_assembly_state,

            /* pipeline 的固定功能 */
            .pViewportState      = &viewport_state,
            .pRasterizationState = &_rasterization_state,
            .pMultisampleState   = &_msaa_state,
            .pDepthStencilState  = &_depth_stencil_state,
            .pColorBlendState    = &_blend_state,
            .pDynamicState       = &dynamic_state,

            .layout = _pipeline_layout,

            .renderPass         = renderpass,
            .subpass            = subpass,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex  = -1,
    };

    vk::Result   result;
    vk::Pipeline pipeline;
    std::tie(result, pipeline) = device.vkdevice().createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
    if (result != vk::Result::eSuccess)
        throw std::runtime_error("fail to create pipeline: " + vk::to_string(result));
    return pipeline;
}


void Hiss::GraphicsPipelineState::shader_stage_add(const vk::PipelineShaderStageCreateInfo& shader_stage)
{
    _shader_stages.push_back(shader_stage);
}


void Hiss::GraphicsPipelineState::viewport_set(const vk::Extent2D& extent)
{
    _viewport.width  = static_cast<float>(extent.width);
    _viewport.height = static_cast<float>(extent.height);
    _scissor.extent  = extent;
}


void Hiss::GraphicsPipelineState::msaa_set(vk::SampleCountFlagBits samples)
{
    assert(samples != vk::SampleCountFlagBits::e1);
    _msaa_state.rasterizationSamples = samples;
    _msaa_state.sampleShadingEnable  = VK_TRUE;
}


void Hiss::GraphicsPipelineState::pipeline_layout_set(vk::PipelineLayout layout)
{
    _pipeline_layout = layout;
}


void Hiss::GraphicsPipelineState::vertex_input_binding_set(
        const std::vector<vk::VertexInputBindingDescription>& bindings)
{
    _vertex_bindind_descriptions = bindings;
}


void Hiss::GraphicsPipelineState::vertex_input_attribute_set(
        const std::vector<vk::VertexInputAttributeDescription>& attributes)
{
    _vertex_attribute_descriptions = attributes;
}


void Hiss::GraphicsPipelineState::dynamic_state_add(vk::DynamicState state)
{
    this->_dynamic_states.push_back(state);
}
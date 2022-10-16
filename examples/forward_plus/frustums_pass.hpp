#pragma once
#include "forward_common.hpp"


namespace ForwardPlus
{

/**
 * 预计算 pass，整个应用开始时执行一次
 * 计算 tile 对应的 frustum 在 view space 中的平面方程参数
 */
struct FrustumsPass : public Hiss::IPass
{
    explicit FrustumsPass(Hiss::Engine& engine, Resource& resource)
        : Hiss::IPass(engine),
          resource(resource)
    {}

    Resource& resource;

    const std::filesystem::path shader_frustum = shader / "forward_plus" / "frustum.comp";

    vk::DescriptorSetLayout descriptor_set_layout;
    vk::PipelineLayout      pipeline_layout;
    vk::Pipeline            pipeline;

    vk::DescriptorSet descriptor_set;
    vk::CommandBuffer command_buffer = g_engine->device().create_commnad_buffer("frustum-pass");


    void prepare()
    {
        descriptor_set_layout = Hiss::Initial::descriptor_set_layout(
                g_engine->vkdevice(), {
                                              {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute},
                                              {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute},
                                      });

        pipeline_layout = Hiss::Initial::pipeline_layout(g_engine->vkdevice(), {descriptor_set_layout});

        descriptor_set = g_engine->create_descriptor_set(descriptor_set_layout, "frustum-pass");


        // shader stage，并且传入一下编译时常量
        auto shader_stage = engine.shader_loader().load(shader_frustum, vk::ShaderStageFlagBits::eCompute);
        shader_stage.pSpecializationInfo = &resource.specilazatin_info;

        pipeline = Hiss::Initial::compute_pipeline(g_engine->vkdevice(), pipeline_layout, shader_stage);


        // 将 buffer 和 descriptor 绑定起来
        Hiss::Initial::descriptor_set_write(
                g_engine->vkdevice(), descriptor_set,
                {
                        {.type = vk::DescriptorType::eStorageBuffer, .buffer = &resource.frustums_ssbo},
                        {.type = vk::DescriptorType::eUniformBuffer, .buffer = &resource.scene_uniform},
                });


        exec_command();
    }


    void exec_command()
    {
        // 录制命令
        command_buffer.begin(vk::CommandBufferBeginInfo{});
        {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_set},
                                              {});
            // 每个 thread 负责一个 tile
            const uint32_t group_count_x = (tile_num_x() + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
            const uint32_t group_count_y = (tile_num_y() + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
            command_buffer.dispatch(group_count_x, group_count_y, 1);
        }
        command_buffer.end();


        // 立即执行命令
        auto fence = engine.device().fence_pool().acquire();
        g_engine->queue().submit_commands({}, {command_buffer}, {}, fence);
        auto result = engine.vkdevice().waitForFences({fence}, VK_TRUE, UINT64_MAX);
        vk::resultCheck(result, fmt::format("fail to exec frustum pass, res").c_str());
    }


    void clean() const
    {
        g_engine->vkdevice().destroy(descriptor_set_layout);
        g_engine->vkdevice().destroy(pipeline_layout);
        g_engine->vkdevice().destroy(pipeline);
    }
};

}    // namespace ForwardPlus
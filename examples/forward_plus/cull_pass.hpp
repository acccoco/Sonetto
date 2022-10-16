#pragma once
#include "forward_common.hpp"


namespace ForwardPlus
{

/**
 * 用于剔除场景中的光源，为每个 tile 生成 light list
 */
struct CullPass : public Hiss::IPass
{
    explicit CullPass(Hiss::Engine& engine, Resource& resource)
        : Hiss::IPass(engine),
          resource(resource)
    {}

    Resource& resource;

    const std::filesystem::path shader_light_cull = shader / "forward_plus" / "light_cull.comp";

    vk::DescriptorSetLayout descriptor_set_layout;
    vk::PipelineLayout      pipeline_layout;
    vk::Pipeline            pipeline;


    struct DebugData    // std430
    {
        alignas(4) float min_depth;
        alignas(4) float max_depth;
        float min_depth_vs;
        float max_depth_vs;
        alignas(4) uint32_t light_index_offset;
        alignas(4) uint32_t light_index_count;
    };


    struct Payload
    {
        vk::DescriptorSet descriptor_set;
        vk::CommandBuffer command_buffer = g_engine->device().create_commnad_buffer("cull-pass");


        /**
     * 原子计数器，在 light cull pass 的 compute shader 中进行读写
     * @details 每一帧使用后都需要将数值清零，因此 application 阶段可以直接写入
     */
        Hiss::Buffer atomic_counter{g_engine->device(),
                                    g_engine->allocator,
                                    sizeof(uint32_t),
                                    vk::BufferUsageFlagBits::eStorageBuffer,
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                            | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                                            | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                    "atomic-counter"};


        /**
         * 用于调试的 buffer，记录每个 tile 的一些运算细节
         */
        Hiss::Buffer debug_buffer{g_engine->device(),
                                  g_engine->allocator,
                                  sizeof(DebugData) * tile_num(),
                                  vk::BufferUsageFlagBits::eStorageBuffer,
                                  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                  "debug-buffer"};
    };

    std::vector<Payload> payloads{g_engine->frame_manager().frames_number()};


    /**
     * depth buffer 使用的 sampler，没什么特殊的
     */
    vk::Sampler depth_sampler = Hiss::Initial::sampler(g_engine->device());


    void prepare()
    {
        // 创建 descriptor set layout
        descriptor_set_layout = Hiss::Initial::descriptor_set_layout(
                g_engine->vkdevice(),
                {
                        // binding 0: depth texture
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute},
                        // binding 1: light index grid
                        {vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute},
                        // binding 2: scene ubo
                        {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute},
                        // binding 3: frustums ssbo
                        {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute},
                        // binding 4: light list
                        {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute},
                        // binding 5: light index list
                        {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute},
                        // binding 6: atomic counter
                        {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute},
                        // binding 7: debug ssbo
                        {vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute},
                });


        // 创建 pipeline layout
        pipeline_layout = Hiss::Initial::pipeline_layout(g_engine->vkdevice(), {descriptor_set_layout});


        // 创建 pipeline
        auto shader_stage = g_engine->shader_loader().load(shader_light_cull, vk::ShaderStageFlagBits::eCompute);
        pipeline          = Hiss::Initial::compute_pipeline(g_engine->vkdevice(), pipeline_layout, shader_stage);


        // 创建 descriptor set
        for (auto& payload: payloads)
            payload.descriptor_set = g_engine->create_descriptor_set(descriptor_set_layout, "cull-pass");


        // 绑定 descriptor set 与 buffer
        for (int i = 0; i < payloads.size(); ++i)
        {
            Hiss::Initial::descriptor_set_write(
                    g_engine->vkdevice(), payloads[i].descriptor_set,
                    {
                            // binding 0: depth texture
                            {.type    = vk::DescriptorType::eCombinedImageSampler,
                             .image   = &resource.payloads[i].depth_attach,
                             .sampler = depth_sampler},
                            // binding 1: light index grid
                            {.type = vk::DescriptorType::eStorageImage, .image = &resource.payloads[i].light_grid_ssio},
                            // binding 2: scene ubo
                            {.type = vk::DescriptorType::eUniformBuffer, .buffer = &resource.scene_uniform},
                            // binding 3: frustums
                            {.type = vk::DescriptorType::eStorageBuffer, .buffer = &resource.frustums_ssbo},
                            // binding 4: light list
                            {.type = vk::DescriptorType::eStorageBuffer, .buffer = &resource.lights_ssbo},
                            // binding 5: light index list
                            {.type   = vk::DescriptorType::eStorageBuffer,
                             .buffer = &resource.payloads[i].light_index_ssbo},
                            // binding 6: atomic counter
                            {.type = vk::DescriptorType::eStorageBuffer, .buffer = &payloads[i].atomic_counter},
                            // binding 7: debug ssbo
                            {.type = vk::DescriptorType::eStorageBuffer, .buffer = &payloads[i].debug_buffer},
                    });
        }


        // 录制命令
        record_command();
    }


    /**
     * 将原子计数器的数值清零
     */
    void clear_atomic_counter()
    {
        auto&    frame = g_engine->current_frame();
        uint32_t data  = 0;
        payloads[frame.frame_id()].atomic_counter.mem_copy(&data, sizeof(data));
    }


    /**
     * 只用录制一次命令即可
     */
    void record_command()
    {
        for (int i = 0; i < payloads.size(); ++i)
        {
            auto& payload        = payloads[i];
            auto& shared_payload = resource.payloads[i];


            // 录制命令
            payload.command_buffer.begin(vk::CommandBufferBeginInfo{});
            {
                // 等待前一个 depth pass 完成深度写入，并且进行 layout 转换
                shared_payload.depth_attach.memory_barrier(
                        {vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
                         vk::AccessFlagBits::eDepthStencilAttachmentWrite},
                        {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderRead},
                        vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                        payload.command_buffer);


                // 执行当前 pass
                payload.command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
                payload.command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0,
                                                          {payload.descriptor_set}, {});


                // 每个 workgroup 负责一个 tile，每个 thread 负责一个 pixel
                payload.command_buffer.dispatch(tile_num_x(), tile_num_y(), 1);
            }
            payload.command_buffer.end();
        }
    }


    std::vector<bool> has_run = std::vector<bool>(engine.frame_manager().frames_number(), false);


    void update()
    {
        auto& frame   = g_engine->current_frame();
        auto& payload = payloads[frame.frame_id()];


        //        if (has_run[frame.frame_id()])
        //            return;
        //        else
        //            has_run[frame.frame_id()] = true;


        // 原子计数器清零
        clear_atomic_counter();


        // 执行录制好的命令。使用 barrier 同步，无需 semaphore
        g_engine->device().queue().submit_commands({}, {payload.command_buffer}, {}, frame.insert_fence());
    }


    void clean() const
    {
        g_engine->vkdevice().destroy(descriptor_set_layout);
        g_engine->vkdevice().destroy(pipeline_layout);
        g_engine->vkdevice().destroy(pipeline);

        g_engine->vkdevice().destroy(depth_sampler);
    }
};

}    // namespace ForwardPlus

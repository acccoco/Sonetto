#pragma once
#include "bloom_common.hpp"
#include "utils/tools.hpp"
#include "utils/application.hpp"
#include "utils/vk_func.hpp"
#include "utils/pipeline_template.hpp"
#include "proj_config.hpp"


namespace Bloom
{
class BloomPass : public Hiss::IPass
{
public:
    struct Resource
    {
        /**
         * 初次渲染好的场景
         * format: sfloat
         * layout: shader_readonly
         * usage: sampled
         */
        std::shared_ptr<Hiss::Image2D> frag_color;

        /**
         * 最终呈现出的颜色
         * format: sRGB
         * layout: general
         * usage: storage
         */
        std::shared_ptr<Hiss::Image2D> bloom_color;
    };


    explicit BloomPass(Hiss::Engine& engine)
        : Hiss::IPass(engine)
    {}


    void prepare(const std::vector<Resource>& resources)
    {
        assert(resources.size() == payloads.size());
        for (int i = 0; i < payloads.size(); ++i)
            payloads[i].resource = resources[i];

        create_descriptor();
        create_pipeline();

        record_command();
    }


    void update()
    {
        auto&& frame = engine.current_frame();
        engine.queue().submit_commands({}, {command_buffer}, {}, frame.insert_fence());
    }


    void clean()
    {
        engine.vkdevice().destroy(pipeline_layout);
        engine.vkdevice().destroy(pipeline);
        engine.vkdevice().destroy(color_sampler);
    }


private:
    struct Payload
    {
        vk::DescriptorSet descriptor_set;
        Resource          resource;
    };

    const std::filesystem::path shader_path = shader / "bloom" / "gauss.comp";

    vk::Pipeline            pipeline;
    vk::PipelineLayout      pipeline_layout;
    vk::DescriptorSetLayout descriptor_layout;
    vk::CommandBuffer       command_buffer = engine.device().create_commnad_buffer("bloom pass command buffer");
    std::vector<Payload>    payloads{engine.frame_manager().frames_number()};
    vk::Sampler             color_sampler = Hiss::Initial::sampler(engine.device());


    void create_descriptor()
    {
        descriptor_layout = Hiss::Initial::descriptor_set_layout(
                engine.vkdevice(),
                {
                        {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute},
                        {vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute},
                });


        for (int i = 0; i < payloads.size(); ++i)
        {
            payloads[i].descriptor_set =
                    engine.create_descriptor_set(descriptor_layout, fmt::format("bloom pass descriptor {}", i));
        }


        // 写入
        for (auto & payload : payloads)
        {
            Hiss::Initial::descriptor_set_write(engine.vkdevice(), payload.descriptor_set,
                                                {
                                                        {.type    = vk::DescriptorType::eCombinedImageSampler,
                                                         .image   = payload.resource.frag_color.get(),
                                                         .sampler = color_sampler},
                                                        {.type  = vk::DescriptorType::eStorageImage,
                                                         .image = payload.resource.bloom_color.get()},
                                                });
        }
    }

    void create_pipeline()
    {
        pipeline_layout = Hiss::Initial::pipeline_layout(engine.vkdevice(), {descriptor_layout});

        auto shader_stage = engine.shader_loader().load(shader_path, vk::ShaderStageFlagBits::eCompute);
        pipeline          = Hiss::Initial::compute_pipeline(engine.vkdevice(), pipeline_layout, shader_stage);
    }


    void record_command()
    {
        command_buffer.begin(vk::CommandBufferBeginInfo{});
        {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {}, {});

            uint32_t group_count_x = ROUND(engine.extent().width, WORKGROUP_SIZE);
            uint32_t group_count_y = ROUND(engine.extent().height, WORKGROUP_SIZE);
            command_buffer.dispatch(group_count_x, group_count_y, 1);
        }
        command_buffer.end();
    }
};
}    // namespace Bloom
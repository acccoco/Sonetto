#pragma once
#pragma once
#include "engine.hpp"
#include "proj_config.hpp"
#include "vk/texture.hpp"
#include "vk/pipeline.hpp"
#include "application.hpp"
#include "vk/vk_config.hpp"

#include "./particle.hpp"


namespace ParticleCompute
{


// 函数曲面这个应用所需的 compute 相关资源
// 每个粒子是一个 Particle 类的实例，共有 dim * dim 个粒子
struct Surface
{


#pragma region layout 相关


    struct Specialization
    {
        const uint32_t dim        = 256;                  // constant id 0 一个方向上粒子的数量，正方形
        const float    step       = 2.f / (float) dim;    // constant id 1
        const uint32_t group_size = 8;    // constant id 2 workgroup 一个方向上的尺寸，正方形
    } specialization;


    struct
    {
        const std::filesystem::path                   path    = shader / "compute_particle/surface.comp";
        const vk::ShaderStageFlagBits                 stage   = vk::ShaderStageFlagBits::eCompute;
        const std::vector<vk::SpecializationMapEntry> entries = {
                {0, offsetof(Specialization, dim), sizeof(Specialization::dim)},
                {1, offsetof(Specialization, step), sizeof(Specialization::step)},
                {2, offsetof(Specialization, group_size), sizeof(Specialization::group_size)},
                {3, offsetof(Specialization, group_size), sizeof(Specialization::group_size)},
        };
    } shader_info;


    struct PushConstant
    {
        float    time{};
        uint32_t kernel_id      = 0;      // 一个 0-9 可以选择
        float    trans_progress = 0.f;    // 转变进度
    };


    std::vector<vk::DescriptorSetLayoutBinding> descriptor_set_bindings = {
            {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    };


    std::vector<vk::PushConstantRange> push_constant_ranges = {
            {vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstant)},
    };


    vk::PipelineLayout      pipeline_layout;
    vk::Pipeline            pipeline;
    vk::DescriptorSetLayout descriptor_set_layout;


#pragma endregion


    struct Payload
    {
        vk::DescriptorSet descriptor_set;
        vk::CommandBuffer command_buffer;
    };


    const uint32_t KERNEL_NUM                = 10;    // compute shader 中 kernel 的数量
    uint32_t       kernel_id                 = 0;
    const float    MAX_SURFACE_DURATION_TIME = 3.f;    // 一个曲面的持续时间

    float surface_duration_time = 0.f;    // 当前曲面持续了多长时间


    Hiss::Buffer2* storage_buffer = nullptr;

    std::vector<Payload> payloads;


    Hiss::Engine& engine;


#pragma region 公开的接口
public:
    explicit Surface(Hiss::Engine& engine)
        : engine(engine)
    {}

    void prepare()
    {
        create_pipeline();

        create_storage_buffer();

        // 初始化 payload
        payloads.resize(engine.frame_manager().frames_number());
        for (auto& paylod: payloads)
            paylod.command_buffer = engine.device().command_pool().command_buffer_create().front();

        create_descriptor_set();

        bind_descriptor_set();
    }


    void update()
    {
        auto& frame   = engine.current_frame();
        auto& payload = payloads[frame.frame_id()];

        // 重新录制命令，为了传入 push constant
        payload.command_buffer.reset();
        record_command(payload);

        // 同一个 queue，使用 pipeline barrier 同步，无需 semaphore
        engine.queue().submit_commands({}, {payload.command_buffer}, {}, frame.insert_fence());
    }


    void clean() const
    {
        engine.vkdevice().destroy(descriptor_set_layout);
        engine.vkdevice().destroy(pipeline_layout);
        engine.vkdevice().destroy(pipeline);
        delete storage_buffer;
    }


    [[nodiscard]] uint32_t num_particles() const { return specialization.dim * specialization.dim; }
#pragma endregion


private:
    // 将 descriptor 与 buffer 绑定起来
    void bind_descriptor_set()
    {
        assert(!payloads.empty());
        assert(storage_buffer);


        auto buffer_info = vk::DescriptorBufferInfo{
                storage_buffer->vkbuffer(),
                0,
                VK_WHOLE_SIZE,
        };

        for (auto& payload: payloads)
        {
            assert(payload.descriptor_set);


            engine.vkdevice().updateDescriptorSets(
                    vk::WriteDescriptorSet{
                            .dstSet          = payload.descriptor_set,
                            .dstBinding      = 0,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType  = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo     = &buffer_info,
                    },
                    {});
        }
    }


    void create_pipeline()
    {
        // descriptor set layout
        descriptor_set_layout = engine.vkdevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = (uint32_t) descriptor_set_bindings.size(),
                .pBindings    = descriptor_set_bindings.data(),
        });


        // pipeline layout
        pipeline_layout = engine.vkdevice().createPipelineLayout((vk::PipelineLayoutCreateInfo{
                .setLayoutCount         = 1,
                .pSetLayouts            = &descriptor_set_layout,
                .pushConstantRangeCount = (uint32_t) push_constant_ranges.size(),
                .pPushConstantRanges    = push_constant_ranges.data(),
        }));


        // shader
        auto shader       = engine.shader_loader().load(shader_info.path, shader_info.stage);
        auto special_info = vk::SpecializationInfo{
                .mapEntryCount = (uint32_t) shader_info.entries.size(),
                .pMapEntries   = shader_info.entries.data(),
                .dataSize      = sizeof(specialization),
                .pData         = &specialization,
        };
        shader.pSpecializationInfo = &special_info;


        // pipeline
        vk::Result result;
        std::tie(result, pipeline) = engine.vkdevice().createComputePipeline(nullptr, vk::ComputePipelineCreateInfo{
                                                                                              .stage  = shader,
                                                                                              .layout = pipeline_layout,
                                                                                      });
        vk::resultCheck(result, "compute pipeline create.");
    }

    // 录制命令
    void record_command(Payload& payload)
    {
        uint32_t group_num = (specialization.dim + specialization.group_size - 1) / specialization.group_size;


        auto& command_buffer = payload.command_buffer;
        command_buffer.begin(vk::CommandBufferBeginInfo{});


        // 使用 storage buffer 之前，插入 execution barrier
        storage_buffer->memory_barrier(command_buffer, {vk::PipelineStageFlagBits::eVertexInput},
                                       {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite});


        command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {payload.descriptor_set},
                                          nullptr);

        PushConstant data = {.time = (float) engine.timer().now_ms() / 1000.f};
        {
            if (surface_duration_time > MAX_SURFACE_DURATION_TIME)
            {
                surface_duration_time = 0.f;
                kernel_id             = (kernel_id + 1) % KERNEL_NUM;
            }
            data.kernel_id = kernel_id;
            surface_duration_time += (float) engine.timer().duration_ms() / 1000.f;
            data.trans_progress = surface_duration_time / MAX_SURFACE_DURATION_TIME;
        }
        command_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(data), &data);
        command_buffer.dispatch(group_num, group_num, 1);


        command_buffer.end();
    }


    void create_descriptor_set()
    {
        assert(descriptor_set_layout);


        // descriptor set
        auto layouts         = std::vector<vk::DescriptorSetLayout>(payloads.size(), descriptor_set_layout);
        auto descriptor_sets = engine.vkdevice().allocateDescriptorSets({
                .descriptorPool     = engine.descriptor_pool(),
                .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
                .pSetLayouts        = layouts.data(),
        });
        for (int i = 0; i < payloads.size(); ++i)
            payloads[i].descriptor_set = descriptor_sets[i];
    }


    // 创建 storage buffer，无需填入初始数据
    void create_storage_buffer()
    {
        vk::DeviceSize size = specialization.dim * specialization.dim * sizeof(Particle);

        // 只需要创建出来即可，不需要初始化
        storage_buffer =
                new Hiss::Buffer2(engine.allocator, size,
                                  vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                                  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    }
};

}    // namespace ParticleCompute
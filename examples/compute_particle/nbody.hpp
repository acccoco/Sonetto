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


/**
 * NBody 粒子效果模拟的 compute 部分
 */
struct NBody
{
    struct UBO    // std140
    {
        alignas(4) float delta_time;
        alignas(4) int32_t particle_count;
    };


    // shader 中的常量数据，需要在 shader 的编译期传入
    struct Specialization
    {
        uint32_t    workgroup_size   = {};        // constant id 0
        uint32_t    shared_data_size = {};        // constant id 1，shader 之间共享数据的大小
        const float gravity          = 0.002f;    // constant id 2
        const float power            = 0.75f;     // constant id 3
        const float soften           = 0.05f;     // constant id 4
    };


#pragma region 成员字段
public:
    /* 一些配置信息 */
    uint32_t workgroup_size   = {};
    uint32_t workgroup_num    = {};
    uint32_t shared_data_size = {};    // 单位是 sizeof(glm::vec4)

    const std::filesystem::path shader_file_calculate = shader / "compute_particle/calculate.comp";
    const std::filesystem::path shader_file_integrate = shader / "compute_particle/integrate.comp";

    struct Payload
    {
        vk::DescriptorSet    descriptor_set = VK_NULL_HANDLE;
        vk::CommandBuffer    command_buffer = VK_NULL_HANDLE;
        Hiss::UniformBuffer* uniform_buffer = nullptr;
    };
    std::vector<Payload> payloads;


    vk::DescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    vk::PipelineLayout      pipeline_layout       = VK_NULL_HANDLE;    // 两个 pipeline 的 layout 相同
    vk::Pipeline            pipeline_calculate    = VK_NULL_HANDLE;    // 计算质点受力，更新质点速度
    vk::Pipeline            pipeline_intergrate   = VK_NULL_HANDLE;    // 更新质点的位置

    UBO ubo = {};

    Hiss::Engine&  engine;
    Hiss::Buffer2* storage_buffer = nullptr;

    // 粒子相关的变量
    const uint32_t        PARTICLES_PER_ATTRACTOR = 4 * 1024;
    uint32_t              num_particles           = {};
    std::vector<Particle> particles               = {};


    const std::vector<vk::DescriptorSetLayoutBinding> compute_descriptor_bindings = {{
            {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
            {1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    }};
#pragma endregion


#pragma region 对外的接口
public:
    explicit NBody(Hiss::Engine& engine)
        : engine(engine)
    {}


    void prepare()
    {
        init_particles();
        init_storage_buffer();


        /* workgroup 相关的数据 */
        auto max_shared_data_size = engine.gpu().properties().limits.maxComputeSharedMemorySize;
        workgroup_size   = std::min<uint32_t>(256, engine.gpu().properties().limits.maxComputeWorkGroupSize[0]);
        shared_data_size = std::min<uint32_t>(1024, max_shared_data_size / sizeof(glm::vec4));
        workgroup_num    = num_particles / workgroup_size;
        spdlog::info("workgroup size: {}, workgroup count: {}, shared data size: {}", workgroup_size, workgroup_num,
                     shared_data_size);
        spdlog::info("work group shared memory size: {} bytes", max_shared_data_size);


        // 初始化每一帧需要用到的数据
        payloads = {engine.frame_manager().frames().size(), Payload()};
        for (int i = 0; i < payloads.size(); ++i)
        {
            auto& payload = payloads[i];

            // 注意 semaphore 应该是 signaled
            payload.command_buffer =
                    engine.device().command_pool().command_buffer_create(fmt::format("compute[{}]", i));
        }


        prepare_uniform_buffer();
        prepare_descriptor_set();
        prepare_pipeline();


        /* 录制 command buffer */
        for (auto& payload: payloads)
            record_command(payload.command_buffer, payload.descriptor_set);
    }


    void update()
    {
        auto& frame   = engine.current_frame();
        auto& payload = payloads[frame.frame_id()];


        double delta_time = engine.timer().duration_ms() / 1000.0;    // 以秒为单位
        update_uniform_buffer(*payload.uniform_buffer, (float) delta_time);


        // compute 阶段，不用重新录制 command buffer
        engine.queue().submit_commands({}, {payload.command_buffer}, {}, frame.insert_fence());
    }


    void clean()
    {
        spdlog::info("compute clean");

        delete storage_buffer;
        engine.vkdevice().destroy(descriptor_set_layout);
        engine.vkdevice().destroy(pipeline_layout);
        engine.vkdevice().destroy(pipeline_intergrate);
        engine.vkdevice().destroy(pipeline_calculate);
        for (auto& payload: payloads)
        {
            DELETE(payload.uniform_buffer);
        }

        spdlog::info("compute clean complete");
    }

#pragma endregion


#pragma region 成员初始化方法
private:
    // 创建 uniform buffer，并填入初始数据
    void prepare_uniform_buffer()
    {
        assert(num_particles);

        /* init ubo */
        ubo.particle_count = static_cast<int32_t>(num_particles);
        ubo.delta_time     = 0.f;


        /* init uniform buffers */
        for (auto& payload: payloads)
        {
            payload.uniform_buffer = new Hiss::UniformBuffer(engine.allocator, sizeof(ubo));
            payload.uniform_buffer->memory_copy(&ubo, sizeof(ubo));
        }
    }


    void prepare_descriptor_set()
    {
        /* descriptor set layout */
        descriptor_set_layout = engine.vkdevice().createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = static_cast<uint32_t>(compute_descriptor_bindings.size()),
                .pBindings    = compute_descriptor_bindings.data(),
        });


        /* descriptor sets */
        std::vector<vk::DescriptorSetLayout> temp_layout(payloads.size(), descriptor_set_layout);
        auto descriptor_sets = engine.vkdevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                .descriptorPool     = engine.descriptor_pool(),
                .descriptorSetCount = static_cast<uint32_t>(temp_layout.size()),
                .pSetLayouts        = temp_layout.data(),
        });
        for (int i = 0; i < payloads.size(); ++i)
            payloads[i].descriptor_set = descriptor_sets[i];


        /* 将 descriptor 和 buffer 绑定起来 */
        assert(storage_buffer);
        for (auto& payload: payloads)
        {
            vk::DescriptorBufferInfo storage_buffer_info = {storage_buffer->vkbuffer(), 0, VK_WHOLE_SIZE};
            vk::DescriptorBufferInfo uniform_buffer_info = {payload.uniform_buffer->vkbuffer(), 0, VK_WHOLE_SIZE};

            std::array<vk::WriteDescriptorSet, 2> descriptor_writes = {{
                    {
                            .dstSet          = payload.descriptor_set,
                            .dstBinding      = 0,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType  = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo     = &storage_buffer_info,
                    },
                    {
                            .dstSet          = payload.descriptor_set,
                            .dstBinding      = 1,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType  = vk::DescriptorType::eUniformBuffer,
                            .pBufferInfo     = &uniform_buffer_info,
                    },
            }};
            engine.vkdevice().updateDescriptorSets(descriptor_writes, {});
        }
    }


    void prepare_pipeline()
    {
        /* pipeline layout */
        assert(descriptor_set_layout);
        pipeline_layout = engine.vkdevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 1,
                .pSetLayouts    = &descriptor_set_layout,
        });


        /* 1st pipeline: calculate */
        create_pipeline_culculate();


        /* 2nd pipeline: integrate */
        create_pipeline_integrate();
    }


    void create_pipeline_culculate()
    {
        /* shader, and specialization constant */
        std::array<vk::SpecializationMapEntry, 5> specialization_entries = {{
                {0, offsetof(Specialization, workgroup_size), sizeof(Specialization::workgroup_size)},
                {1, offsetof(Specialization, shared_data_size), sizeof(Specialization::shared_data_size)},
                {2, offsetof(Specialization, gravity), sizeof(Specialization::gravity)},
                {3, offsetof(Specialization, power), sizeof(Specialization::power)},
                {4, offsetof(Specialization, soften), sizeof(Specialization::soften)},
        }};

        auto specialization_data = Specialization{
                .workgroup_size   = workgroup_size,
                .shared_data_size = shared_data_size,
        };

        vk::SpecializationInfo specialization_info = {
                static_cast<uint32_t>(specialization_entries.size()),
                specialization_entries.data(),
                sizeof(specialization_data),
                &specialization_data,
        };
        auto shader_stage = engine.shader_loader().load(shader_file_calculate, vk::ShaderStageFlagBits::eCompute);
        shader_stage.pSpecializationInfo            = &specialization_info;
        vk::ComputePipelineCreateInfo pipeline_info = {
                .stage  = shader_stage,
                .layout = pipeline_layout,
        };


        /* generate pipeline */
        vk::Result result;
        std::tie(result, pipeline_calculate) = engine.vkdevice().createComputePipeline(VK_NULL_HANDLE, pipeline_info);
        vk::resultCheck(result, "failed to create compute pipeline: calculate.");
    }


    void create_pipeline_integrate()
    {
        /* shader, and specialization */
        vk::SpecializationMapEntry specialization_entry = {0, 0, sizeof(workgroup_size)};
        vk::SpecializationInfo     specialization_info  = {
                1,
                &specialization_entry,
                sizeof(workgroup_size),
                &workgroup_size,
        };

        vk::ComputePipelineCreateInfo pipeline_info = {
                .stage  = engine.shader_loader().load(shader_file_integrate, vk::ShaderStageFlagBits::eCompute),
                .layout = pipeline_layout,
        };
        pipeline_info.stage.pSpecializationInfo = &specialization_info;

        /* generate pipeline */
        vk::Result result;
        std::tie(result, pipeline_intergrate) = engine.vkdevice().createComputePipeline(VK_NULL_HANDLE, pipeline_info);
        vk::resultCheck(result, "failed to create compute pipeline: integrate");
    }


    void record_command(vk::CommandBuffer command_buffer, vk::DescriptorSet descriptor_set) const
    {
        command_buffer.begin(vk::CommandBufferBeginInfo{});


        // 从 graphics 阶段获取 storage buffer
        assert(storage_buffer);
        storage_buffer->memory_barrier(
                command_buffer, {vk::PipelineStageFlagBits::eVertexInput, vk::AccessFlagBits::eVertexAttributeRead},
                {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite});


        /* 1st pass: 计算受力，更新速度 */
        command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline_calculate);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_set},
                                          nullptr);
        command_buffer.dispatch(workgroup_num, 1, 1);


        /* 在 1st 和 2nd 之间插入 memory barrier */
        storage_buffer->memory_barrier(command_buffer,
                                       {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite},
                                       {vk::PipelineStageFlagBits::eComputeShader,
                                        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite});


        /* 2nd pass: 更新质点的位置 */
        command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline_intergrate);
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {descriptor_set},
                                          nullptr);
        command_buffer.dispatch(workgroup_num, 1, 1);


        command_buffer.end();
    }


    void update_uniform_buffer(Hiss::UniformBuffer& buffer, float delta_time)
    {
        ubo.delta_time = delta_time;

        buffer.memory_copy(&ubo, sizeof(ubo));
    }


    // 给出最初的粒子的属性
    void init_particles()
    {
        spdlog::info("particles craete");

        std::vector<glm::vec3> attractors = {
                glm::vec3(5.0f, 0.0f, 0.0f),     //
                glm::vec3(-5.0f, 0.0f, 0.0f),    //
                glm::vec3(0.0f, 0.0f, 5.0f),     //
                glm::vec3(0.0f, 0.0f, -5.0f),    //
                glm::vec3(0.0f, 4.0f, 0.0f),     //
                glm::vec3(0.0f, -8.0f, 0.0f),    //
        };
        spdlog::info("number of attractors: {}", attractors.size());

        num_particles = attractors.size() * PARTICLES_PER_ATTRACTOR;
        particles     = std::vector<Particle>(num_particles);
        spdlog::info("particles number: {}", num_particles);

        Hiss::Rand rand;

        /* initial particle position and velocity */
        for (uint32_t attr_idx = 0; attr_idx < static_cast<uint32_t>(attractors.size()); ++attr_idx)
        {
            for (uint32_t p_in_group_idx = 0; p_in_group_idx < PARTICLES_PER_ATTRACTOR; ++p_in_group_idx)
            {
                Particle& particle = particles[attr_idx * PARTICLES_PER_ATTRACTOR + p_in_group_idx];

                /* 每一组中的第一个 particle 是重心 */
                if (p_in_group_idx == 0)
                {
                    particle.pos = glm::vec4(attractors[attr_idx] * 1.5f, 90000.f);
                    particle.vel = glm::vec4(0.f);
                }
                else
                {
                    /* position */
                    glm::vec3 position = attractors[attr_idx]
                                       + 0.75f * glm::vec3(rand.normal_0_1(), rand.normal_0_1(), rand.normal_0_1());
                    float len = glm::length(glm::normalize(position - attractors[attr_idx]));
                    position.y *= 2.f - (len * len);

                    /* velocity */
                    glm::vec3 angular  = glm::vec3(0.5f, 1.5f, 0.5f) * (attr_idx % 2 == 0 ? 1.f : -1.f);
                    glm::vec3 velocity = glm::cross((position - attractors[attr_idx]), angular)
                                       + glm::vec3(rand.normal_0_1(), rand.normal_0_1(), rand.normal_0_1() * 0.025f);

                    float mass   = (rand.normal_0_1() * 0.5f + 0.5f) * 75.f;
                    particle.pos = glm::vec4(position, mass);
                    particle.vel = glm::vec4(velocity, 0.f);
                }


                /* 颜色渐变的 uv 坐标 */
                particle.vel.w = (float) attr_idx * 1.f / (float) attractors.size();
            }
        }
    }


    // 创建 storage buffer，并填入初始值
    void init_storage_buffer()
    {
        assert(!particles.empty());
        spdlog::info("storage buffer prepare");


        /* create storage buffer */
        vk::DeviceSize storage_buffer_size = particles.size() * sizeof(Particle);

        storage_buffer =
                new Hiss::Buffer2(engine.allocator, storage_buffer_size,
                                  vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
                                          | vk::BufferUsageFlagBits::eStorageBuffer,
                                  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
        auto stage_buffer = Hiss::StageBuffer(engine.allocator, storage_buffer_size);
        stage_buffer.mem_copy(particles.data(), storage_buffer_size);


        /* stage buffer -> storage buffer */
        Hiss::OneTimeCommand command_buffer(engine.device(), engine.device().command_pool());
        command_buffer().copyBuffer(stage_buffer.vkbuffer(), storage_buffer->vkbuffer(),
                                    {vk::BufferCopy{.size = storage_buffer_size}});


        command_buffer.exec();
    };
#pragma endregion
};

}    // namespace ParticleCompute
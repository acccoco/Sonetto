#pragma once
#include <application.hpp>
#include "proj_profile.hpp"
#include "vk/texture.hpp"
#include "vk/pipeline.hpp"


struct GraphcisUBO
{
    alignas(16) glm::mat4 projection;
    alignas(16) glm::mat4 view;
    alignas(16) glm::vec2 screen_dim;
    // FIXME 对齐可能出问题
};


struct ComputeUBO
{
    float   delta_time;
    int32_t particle_count;
    // FIXME 对齐可能出问题
};


/* shader 中 specialization 的 constant 量，可以理解为 macro */
struct MovementSpecializationData
{
    uint32_t    workgroup_size   = {};        // constant id 0
    uint32_t    shared_data_size = {};        // constant id 1，shader 之间共享数据的大小
    const float gravity          = 0.002f;    // constant id 2
    const float power            = 0.75f;     // constant id 3
    const float soften           = 0.05f;     // constant id 4
};


class ComputeShaderNBody : public Hiss::Application
{
public:
    ComputeShaderNBody()
        : Application("n_body")
    {}
    ~ComputeShaderNBody() override = default;


    void prepare() override;
    void resize() override;
    void update(double delta_time) noexcept override;
    void clean() override;


    void particles_prepare();
    void descriptor_pool_prepare();
    void storage_buffer_prepare();


    void graphics_prepare();
    void graphics_clean();
    void graphics_load_assets();
    void graphics_uniform_buffer_prepare();
    void graphics_pipeline_prepare();
    void graphics_descriptor_set_prepare();
    void graphcis_command_record(vk::CommandBuffer command_buffer, vk::Framebuffer framebuffer,
                                 vk::DescriptorSet descriptor_set, vk::Image color_image);
    void graphics_uniform_update(Hiss::Buffer& buffer, float delta_time);


    void compute_prepare();
    void compute_clean();
    void compute_uniform_buffer_prepare();
    void compute_descriptor_prepare();
    void compute_pipeline_prepare();
    void compute_command_prepare(vk::CommandBuffer command_buffer, vk::DescriptorSet descriptor_set);
    void compute_uniform_update(Hiss::Buffer& buffer, float delta_time);


    // members =======================================================


private:
    struct Particle
    {
        glm::vec4 pos;    // xyz: position, w: mass
        glm::vec4 vel;    // xyz: velocity, w: uv coord
    };


    const uint32_t        PARTICLES_PER_ATTRACTOR = 4 * 1024;
    uint32_t              num_particles           = {};
    std::vector<Particle> particles               = {};
    vk::DescriptorPool    descriptor_pool         = VK_NULL_HANDLE;
    Hiss::Buffer*         storage_buffer          = nullptr;


    const std::array<vk::DescriptorSetLayoutBinding, 3> graphics_descriptor_bindings = {{
            {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
            {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
            {2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex},
    }};


    /* graphics pass resource */
    struct Graphics
    {
        std::vector<vk::DescriptorSet> descriptor_sets       = {};
        Hiss::GraphicsPipelineTemplate pipeline_state        = {};
        vk::DescriptorSetLayout        descriptor_set_layout = VK_NULL_HANDLE;
        vk::PipelineLayout             pipeline_layout       = VK_NULL_HANDLE;
        vk::Pipeline                   pipeline              = VK_NULL_HANDLE;
        GraphcisUBO                    ubo                   = {};

        std::array<Hiss::Buffer*, IN_FLIGHT_CNT> uniform_buffers = {};
        std::array<vk::Semaphore, IN_FLIGHT_CNT> semaphores      = {};

        const std::string vert_shader_path  = SHADER("compute_Nbody/particle.vert.spv");
        const std::string frag_shader_path  = SHADER("compute_Nbody/particle.frag.spv");
        const std::string tex_particle_path = TEXTURE("compute_Nbody/particle_rgba.png");
        const std::string tex_gradient_path = TEXTURE("compute_Nbody/particle_gradient_rgba.png");
        Hiss::Texture*    tex_particle      = nullptr;
        Hiss::Texture*    tex_gradient      = nullptr;
    } graphics;


    const std::array<vk::DescriptorSetLayoutBinding, 2> compute_descriptor_bindings = {{
            {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
            {1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    }};


    /* compute pass resource */
    struct Compute
    {

        /* 一些配置信息 */
        uint32_t workgroup_size   = {};
        uint32_t workgroup_num    = {};
        uint32_t shared_data_size = {};    // 单位是 sizeof(glm::vec4)

        const std::string shader_file_calculate = SHADER("compute_Nbody/calculate.comp.spv");
        const std::string shader_file_integrate = SHADER("compute_Nbody/integrate.comp.spv");


        std::array<vk::Semaphore, IN_FLIGHT_CNT> semaphores      = {};
        std::vector<vk::DescriptorSet>           descriptor_sets = {};    // 每一帧有一个

        vk::DescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
        vk::PipelineLayout      pipeline_layout       = VK_NULL_HANDLE;    // 两个 pipeline 的 layout 相同
        vk::Pipeline            pipeline_calculate    = VK_NULL_HANDLE;    // 计算质点受力，更新质点速度
        vk::Pipeline            pipeline_intergrate   = VK_NULL_HANDLE;    // 更新质点的位置

        MovementSpecializationData               specialization_data = {};
        std::array<Hiss::Buffer*, IN_FLIGHT_CNT> uniform_buffers     = {};    // 每一帧都有一个
        ComputeUBO                               ubo                 = {};

    } compute;
};
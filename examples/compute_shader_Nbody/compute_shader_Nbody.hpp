#pragma once
#include <engine.hpp>
#include "proj_config.hpp"
#include "vk/texture.hpp"
#include "vk/pipeline.hpp"
#include "application.hpp"
#include "vk/vk_config.hpp"


/**
 * 执行顺序：(graphics, compute)+
 * 由于使用的是同一个 queue，因此 graphics 和 compute 之间只需要 pipeline barrier 做同步即可
 */
namespace NBody
{

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
}; /* shader 中 specialization 的 constant 量，可以理解为 macro */


struct Particle
{
    glm::vec4 pos;    // xyz: position, w: mass
    glm::vec4 vel;    // xyz: velocity, w: uv coord
};


struct MovementSpecializationData
{
    uint32_t    workgroup_size   = {};        // constant id 0
    uint32_t    shared_data_size = {};        // constant id 1，shader 之间共享数据的大小
    const float gravity          = 0.002f;    // constant id 2
    const float power            = 0.75f;     // constant id 3
    const float soften           = 0.05f;     // constant id 4
};


/* graphics pass resource */
struct Graphics
{
    Hiss::PipelineTemplate  pipeline_template     = {};
    vk::DescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    vk::PipelineLayout      pipeline_layout       = VK_NULL_HANDLE;
    vk::Pipeline            pipeline              = VK_NULL_HANDLE;
    GraphcisUBO             ubo                   = {};


    struct Payload
    {
        Hiss::UniformBuffer* uniform_buffer = nullptr;
        // vk::Semaphore        semaphore      = VK_NULL_HANDLE;
        vk::DescriptorSet descriptor_set = VK_NULL_HANDLE;
        vk::CommandBuffer command_buffer = VK_NULL_HANDLE;
    };

    std::vector<Payload> payloads;

    const std::filesystem::path vert_shader_path  = shader_dir / "compute_Nbody/particle.vert";
    const std::filesystem::path frag_shader_path  = shader_dir / "compute_Nbody/particle.frag";
    const std::string           tex_particle_path = TEXTURE("compute_Nbody/particle_rgba.png");
    const std::string           tex_gradient_path = TEXTURE("compute_Nbody/particle_gradient_rgba.png");


    // TODO 改造一下
    Hiss::Texture* tex_particle = nullptr;
    Hiss::Texture* tex_gradient = nullptr;


    Hiss::Image2D* depth_image = nullptr;
};


struct Compute
{
    /* 一些配置信息 */
    uint32_t workgroup_size   = {};
    uint32_t workgroup_num    = {};
    uint32_t shared_data_size = {};    // 单位是 sizeof(glm::vec4)

    const std::filesystem::path shader_file_calculate = shader_dir / "compute_Nbody/calculate.comp";
    const std::filesystem::path shader_file_integrate = shader_dir / "compute_Nbody/integrate.comp";

    struct Payload
    {
        // vk::Semaphore        semaphore      = VK_NULL_HANDLE;
        vk::DescriptorSet    descriptor_set = VK_NULL_HANDLE;
        vk::CommandBuffer    command_buffer = VK_NULL_HANDLE;
        Hiss::UniformBuffer* uniform_buffer = nullptr;
    };
    std::vector<Payload> payloads;


    vk::DescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    vk::PipelineLayout      pipeline_layout       = VK_NULL_HANDLE;    // 两个 pipeline 的 layout 相同
    vk::Pipeline            pipeline_calculate    = VK_NULL_HANDLE;    // 计算质点受力，更新质点速度
    vk::Pipeline            pipeline_intergrate   = VK_NULL_HANDLE;    // 更新质点的位置

    MovementSpecializationData specialization_data = {};
    ComputeUBO                 ubo                 = {};
};


class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {}
    ~App() override = default;


#pragma region 特殊的接口
public:
    void prepare() override;
    void resize() override;
    void update() noexcept override;
    void clean() override;

#pragma endregion


#pragma region 公共的数据准备
private:
    void particles_prepare();
    void storage_buffer_prepare();

#pragma endregion


#pragma region 图形部分的数据准备
private:
    void graphics_prepare();
    void graphics_clean();
    void graphics_resize();

    // 读取纹理资源
    void graphics_load_assets();

    // 创建 uniform buffer，并填入初始的数据
    void graphics_create_uniform_buffer();
    void graphics_pipeline_prepare();

    // 创建 descriptor_set_layou, descriptor_set，将 descriptor_set 与 buffer 绑定起来
    void graphics_prepare_descriptor_set();
    void graphics_record_command(vk::CommandBuffer command_buffer, Hiss::Frame& frame, Graphics::Payload& payload);
    void graphics_update_uniform(Hiss::UniformBuffer& buffer);

    // 更新 struct ubo，里面的 projmatrix 之类的
    void graphics_update_uniform_data();
#pragma endregion


#pragma region compute shader 部分的数据准备
private:
    void compute_prepare();
    void compute_clean();

    // 创建 uniform buffer，并填入初始数据
    void compute_uniform_buffer_prepare();
    void compute_prepare_descriptor_set();
    void compute_pipeline_prepare();
    void compute_record_command(vk::CommandBuffer command_buffer, vk::DescriptorSet descriptor_set);
    void compute_update_uniform(Hiss::UniformBuffer& buffer, float delta_time);
#pragma endregion


#pragma region 公共的数据成员
private:
    const uint32_t        PARTICLES_PER_ATTRACTOR = 4 * 1024;
    uint32_t              num_particles           = {};
    std::vector<Particle> particles               = {};

    Hiss::Buffer2* storage_buffer = nullptr;
#pragma endregion


#pragma region 图形部分的数据成员
private:
    const std::vector<vk::DescriptorSetLayoutBinding> graphics_descriptor_bindings = {{
            {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
            {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
            {2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex},
    }};


    Graphics graphics;
#pragma endregion


#pragma region 计算部分的数据成员
private:
    const std::vector<vk::DescriptorSetLayoutBinding> compute_descriptor_bindings = {{
            {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
            {1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    }};


    /* compute pass resource */
    Compute compute;
#pragma endregion
};
}    // namespace NBody

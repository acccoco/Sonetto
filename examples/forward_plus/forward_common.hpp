#pragma once
#include "utils/application.hpp"
#include "utils/vk_func.hpp"
#include "camera.hpp"
#include "engine/model.hpp"
#include "utils/pipeline_template.hpp"
#include "engine/vertex.hpp"
#include "utils/rand.hpp"
#include "proj_config.hpp"
#include <memory>
#include "utils/application.hpp"
#define HISS_CPP
#include "forward_plus/type.glsl"
#include "shader/common_type.glsl"
#include "shader/light_type.glsl"
#include "shader/material_type.glsl"


extern Hiss::Engine* g_engine;


namespace ForwardPlus
{

const float NEAR = 0.1f;     // 近平面到摄像机的距离
const float FAR  = 200.f;    // 远平面到摄像机的距离


// 场景中的光源数量：均匀分布吧
// FIXME
const uint32_t LIGHT_DIM = 16u;
const uint32_t LIGHT_NUM = LIGHT_DIM * LIGHT_DIM * LIGHT_DIM;

// 场景的大小：[-range, range]^3
const float SCENE_RANGE = 100.f;


/**
 * 通过 specialization 的方式传递给 shader 的编译器常量
 */
struct ShaderConst
{
    float    near      = NEAR;
    float    far       = FAR;
    uint32_t tile_size = ForwardPlusShader::TILE_SIZE;
    uint32_t tile_num_x{};    // framebuffer 在 x 方向上可以划分出多少个 tile
    uint32_t tile_num_y{};
    uint32_t local_size_x = ForwardPlusShader::WORKGROUP_SIZE;    // compute shader workgroup size x
    uint32_t local_size_y = ForwardPlusShader::WORKGROUP_SIZE;    // compute shader workgroup size y


    static std::vector<vk::SpecializationMapEntry> specialization_entries()
    {
        return {
                {0, offsetof(ShaderConst, near), sizeof(ShaderConst::near)},
                {1, offsetof(ShaderConst, far), sizeof(ShaderConst::far)},
                {2, offsetof(ShaderConst, tile_size), sizeof(ShaderConst::tile_size)},
                {3, offsetof(ShaderConst, tile_num_x), sizeof(ShaderConst::tile_num_x)},
                {4, offsetof(ShaderConst, tile_num_y), sizeof(ShaderConst::tile_num_y)},
                {5, offsetof(ShaderConst, local_size_x), sizeof(ShaderConst::local_size_x)},
                {6, offsetof(ShaderConst, local_size_y), sizeof(ShaderConst::local_size_y)},
        };
    }
};


/**
 * framebuffer 在 x 方向上划分出了多少个 tile
 */
inline uint32_t tile_num_x()
{
    return ROUND(g_engine->extent().width, ForwardPlusShader::TILE_SIZE);
}


/**
 * framebuffer 在 y 方向上划分出了多少个 tile
 */
inline uint32_t tile_num_y()
{
    return ROUND(g_engine->extent().height, ForwardPlusShader::TILE_SIZE);
}

/**
 * framebuffer 一共划分出了多少个 tile
 */
inline uint32_t tile_num()
{
    return tile_num_x() * tile_num_y();
}


/**
 * 多个 pass 之间，每一帧都不同的
 */
struct SharedFramePayload
{
    /**
     * light index list 的尺寸。每个 tile 平均拥有 100 个光源，应该是够用的
     */
    const uint32_t LIGHT_INDEX_LIST_SIZE = 100u * tile_num();


    // 各种 buffer =============================================================================================

    /**
     * 每一帧都会变化的数据，直接从 CPU 写入
     * @details 在 compute shader，vertex shader 以及 fragment shader 都会读取
     */
    std::shared_ptr<Hiss::UniformBuffer> perframe_uniform{new Hiss::UniformBuffer{
            g_engine->device(), g_engine->allocator, sizeof(Shader::Frame), "perframe-uniform"}};

    std::shared_ptr<Hiss::DepthAttach> depth_attach{new Hiss::DepthAttach{
            g_engine->allocator, g_engine->device(), g_engine->depth_format(), g_engine->extent(), "depth-attach"}};


    /**
     * 用于存放每个 grid 的 light index list 在全局 light index list 中的 offset 和 count
     * @details - 相当于 vec2 的二维数组，第一个元素为 offset，第二个元素为 count
     * @details - 在 light cull pass 的 compute shader 写入，在 final pass 的 fragment shader 读取
     * @details - 语法参考 https://stackoverflow.com/questions/72962565/what-is-the-syntax-for-passing-a-storage-image-as-descriptor
     */
    std::shared_ptr<Hiss::StorageImage> light_grid_ssio{new Hiss::StorageImage{g_engine->allocator,
                                                                               g_engine->device(),
                                                                               vk::Format::eR32G32Uint,
                                                                               {tile_num_x(), tile_num_y()},
                                                                               "light-grid-storage-image"}};


    /**
     * 所有 tile 的 light list 数据
     * @details 这个数据完全有 GPU 维护：light cull 阶段写入，final 阶段读取
     */
    std::shared_ptr<Hiss::Buffer> light_index_ssbo{new Hiss::Buffer{
            g_engine->device(), g_engine->allocator, sizeof(uint32_t) * LIGHT_INDEX_LIST_SIZE,
            vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, "light-index-ssbo"}};
};


/**
 * 多个 pass 之间共享的资源
 */
struct Resource
{


    // 各种 ubo ===========================================================================================
    /**
     * 更新频率为 frame 的数据
     */
    Shader::Frame frame_ubo = {
            .view_matrix = Camera::view(glm::vec3(30)),
    };

    Shader::Scene scene_ubo = {
            .proj_matrix    = Hiss::perspective(60.f, g_engine->aspect(), NEAR, FAR),
            .in_proj_matrix = glm::inverse(scene_ubo.proj_matrix),
            .screen_width   = g_engine->extent().width,
            .screen_height  = g_engine->extent().height,
            .near           = NEAR,
            .far            = FAR,
            .light_num      = LIGHT_NUM,
    };


    /**
     * 给 shader 准备的编译时常量
     */
    ShaderConst shader_const{
            .tile_num_x = tile_num_x(),
            .tile_num_y = tile_num_y(),
    };

    /**
     * shader 的 specialization 配置
     */
    std::vector<vk::SpecializationMapEntry> entries           = ShaderConst::specialization_entries();
    vk::SpecializationInfo                  specilazatin_info = {
                             .mapEntryCount = (uint32_t) entries.size(),
                             .pMapEntries   = entries.data(),
                             .dataSize      = sizeof(ShaderConst),
                             .pData         = &shader_const,
    };


    /**
     * 在 final pass 的 fragment shader 中会用到，用于确定 pixel 所在 tile 的 id
     */
    struct
    {
        uint32_t tile_num_x;
        uint32_t tile_num_y;
    } tile_num_ubo{tile_num_x(), tile_num_y()};


    /**
     * 场景中所有物体的材质都是相同的
     */
    Shader::Material material = {
            .diffuse_color = {0.5, 0.5, 0.5, 0.5},
    };


    /**
     * 场景中的所有光源，只需要初始化即可
     */
    std::vector<Shader::Light> lights;


    // 各种 buffer =============================================================================================

    /**
     * 每个 tile 的 frustum 平面参数
     * @details 只在应用初始化阶段会调用
     * @details frustum 阶段使用 compute shader 写入数据；light cull 阶段使用 compute shader 读取
     */
    std::shared_ptr<Hiss::Buffer> frustums_ssbo{new Hiss::Buffer{
            g_engine->device(), g_engine->allocator, sizeof(ForwardPlusShader::Frustum) * tile_num(),
            vk::BufferUsageFlagBits::eStorageBuffer, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, "frustum-ssbo"}};

    std::shared_ptr<Hiss::UniformBuffer> scene_uniform{new Hiss::UniformBuffer{
            g_engine->device(), g_engine->allocator, sizeof(Shader::Scene), "scene-data", &scene_ubo}};


    /**
     * 所有光源详细信息的 storage buffer
     * @details 在摄像机位置发生变化时，需要更新，通过 stage buffer 进行更新
     * @details 在 light cull pass 的 compute shader 以及 final 阶段的 fragment shader 时进行读取
     */
    std::shared_ptr<Hiss::Buffer> lights_ssbo{
            new Hiss::Buffer{g_engine->device(), g_engine->allocator, sizeof(Shader::Light) * LIGHT_NUM,
                             vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                             VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, "light-ssbo"}};


    /**
     * 用于向 lights ssbo 中传输数据的 stage buffer
     */
    Hiss::StageBuffer lights_stage_buffer{g_engine->device(), g_engine->allocator, sizeof(Shader::Light) * LIGHT_NUM,
                                          "light-ssbo-stage-buffer"};


    /**
     * 材质的 uniform buffer
     */
    std::shared_ptr<Hiss::UniformBuffer> material_uniform{new Hiss::UniformBuffer{
            g_engine->device(), g_engine->allocator, sizeof(Shader::Material), "material", &material}};


    Hiss::UniformBuffer tile_num_uniform{g_engine->device(), g_engine->allocator, sizeof(tile_num_ubo), "tile-number",
                                         &tile_num_ubo};


    // 每帧都会改变的 buffer =======================================================================================

    std::vector<SharedFramePayload> payloads{g_engine->frame_manager().frames_number()};


    // 模型资源
    Hiss::Mesh mesh_cube{*g_engine, model / "cube/cube.obj", "cube-mesh"};

    // 每个 cube 的位置等
    std::vector<glm::mat4> cube_matrix;
};


}    // namespace ForwardPlus
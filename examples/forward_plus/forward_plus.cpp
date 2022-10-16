#include "forward_common.hpp"
#include "run.hpp"
#include "depth_pass.hpp"
#include "frustums_pass.hpp"
#include "cull_pass.hpp"
#include "final_pass.hpp"


/**
 * 为了简化：
 * \n - 只使用 point 和 方向光
 * \n - tile 大小： 16 x 16，每个 workgroup 有 256 个 thread
 * \n - 暂不考虑透明物体，后续再扩展
 * \n - 阶段
 * \n     - 预计算：compute shader，每个 tile 的 frustum
 * \n     - depth pre-pass：普通的 vertex shader，空的 fragment shader
 * \n     - light culling：compute shader
 * \n     - final shading：普通的 vertex shadr，特殊的 fragment shader，读取 light list
 */
namespace ForwardPlus
{


class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {}
    ~App() override = default;


    Resource     resource;
    DepthPass    depth_pass{engine, resource};
    FrustumsPass frustum_pass{engine, resource};
    CullPass     cull_pass{engine, resource};
    FinalPass    final_pass{engine, resource};


public:
    void prepare() override
    {
        init_light();
        init_cube_matrix();
        init_uniform_buffer();

        // 就更新一次光源，让光源固定
        {
            Hiss::OneTimeCommand cmd(engine.device(), engine.device().command_pool());
            update_light(resource.frame_ubo.view_matrix, cmd());
            cmd.exec();
        }


        // 各个 pass 初始化
        frustum_pass.prepare();
        depth_pass.prepare();
        cull_pass.prepare();
        final_pass.prepare();


        init_test_command_buffer();
    }


    /**
     * 测试用的
     */
    std::vector<vk::CommandBuffer> test_command_buffers{engine.frame_manager().frames_number()};

    /**
     * 测试用的，为了让 swapchain 运转起来
     */
    void init_test_command_buffer()
    {
        for (int i = 0; i < engine.frame_manager().frames_number(); ++i)
        {
            test_command_buffers[i] = engine.device().create_commnad_buffer(fmt::format("test frame {}", i));

            test_command_buffers[i].begin(vk::CommandBufferBeginInfo{});
            Hiss::Engine::color_attach_layout_trans_1(test_command_buffers[i], engine.frame_manager().frame(i).image());
            Hiss::Engine::color_attach_layout_trans_2(test_command_buffers[i], engine.frame_manager().frame(i).image());
            test_command_buffers[i].end();
        }
    }

    void update() override
    {
        engine.frame_manager().acquire_frame();


        // FIXME 临时的
        //        engine.device().queue().submit_commands({}, {test_command_buffers[engine.current_frame().frame_id()]},
        //                                                {engine.current_frame().submit_semaphore()}, engine.current_frame().insert_fence());

        depth_pass.update();
        cull_pass.update();
        final_pass.update();


        engine.frame_manager().submit_frame();
    }

    void clean() override
    {
        frustum_pass.clean();
        depth_pass.clean();
        cull_pass.clean();
        final_pass.clean();
    }


    // region 各种 buffer 的初始化与更新====================================================================


    /**
     * 准备光源数据
     */
    void init_light()
    {
        //        // 光影聚集在 cube 周围
        //        Hiss::Rand rand;
        //        for (int i = -2; i <= 2; ++i)
        //        {
        //            for (int j = -2; j <= 2; ++j)
        //            {
        //                for (int k = -2; k <= 2; ++k)
        //                {
        //                    glm::vec3 pos   = glm::vec3(-0.4, 0.2, -0.3) + glm::vec3(i, j, k) * 0.05f;
        //                    Light     light = {
        //                                .pos_world = glm::vec4(pos * SCENE_RANGE, 1.f),
        //                                .color     = glm::vec4(rand.uniform_0_1(), rand.uniform_0_1(), rand.uniform_0_1(), 1.f),
        //                                .range     = 20.f,
        //                                .intensity = 1.f,
        //                                .type      = POINT_LIGHT,
        //                    };
        //
        //                    resource.lights.push_back(light);
        //                }
        //            }
        //        }


        // 光源是均匀的
        resource.lights.reserve(LIGHT_NUM);
        float      light_interval = 2.f / LIGHT_DIM;
        Hiss::Rand rand;

        for (int i = 0; i < LIGHT_DIM; ++i)
        {
            for (int j = 0; j < LIGHT_DIM; ++j)
            {
                for (int k = 0; k < LIGHT_DIM; ++k)
                {
                    glm::vec3 pos = glm::vec3(-1.f) + glm::vec3(i, j, k) * light_interval;

                    Light light = {
                            .pos_world = glm::vec4(pos * SCENE_RANGE, 1.f),
                            .color     = glm::vec4(rand.uniform_0_1(), rand.uniform_0_1(), rand.uniform_0_1(), 1.f),
                            .range     = 20.f,
                            .intensity = 1.f,
                            .type      = POINT_LIGHT,
                    };

                    resource.lights.push_back(light);
                }
            }
        }


        // 光源是随机的
        //        Hiss::Rand rand;
        //        resource.lights.resize(LIGHT_NUM);
        //        for (auto& light: resource.lights)
        //        {
        //            glm::vec3 pos = glm::vec3(rand.uniform_np1(), rand.uniform_np1(), rand.uniform_np1());
        //
        //            light = {
        //                    .pos_world = glm::vec4(pos * SCENE_RANGE, 1.f),
        //                    .color     = glm::vec4(rand.uniform_0_1(), rand.uniform_0_1(), rand.uniform_0_1(), 1.f),
        //                    .range     = 20.f,
        //                    .intensity = 1.f,
        //                    .type      = POINT_LIGHT,
        //            };
        //        }
    }


    /**
     * 更新光源数据，并将数据写入 storage buffer
     */
    void update_light(const glm::mat4& view_matrix, vk::CommandBuffer command_buffer)
    {
        // view matrix 发生了变换，光源在 view space 的状态也会发生变化
        for (auto& light: resource.lights)
            light.pos_view = view_matrix * light.pos_world;

        resource.lights_stage_buffer.mem_copy(resource.lights.data(), sizeof(Light) * LIGHT_NUM);


        // 将数据写入 storage buffer 中，并添加 barrier
        resource.lights_ssbo.memory_barrier(
                command_buffer,
                {vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader, {}},
                {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite});

        command_buffer.copyBuffer(resource.lights_stage_buffer.vkbuffer(), resource.lights_ssbo.vkbuffer(),
                                  {vk::BufferCopy{.size = resource.lights_ssbo.size()}});

        resource.lights_ssbo.memory_barrier(
                command_buffer, {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite},
                {vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader,
                 vk::AccessFlagBits::eShaderRead});
    }


    /**
     * 将每一帧都会变化的值写入 perframe uniform buffer，暂时不用改变
     */
    void init_uniform_buffer()
    {
        for (auto& payload: resource.payloads)
        {
            payload.perframe_uniform.mem_copy(&resource.frame_ubo, sizeof(resource.frame_ubo));
        }
    }


    /**
     * 初始化各个 cube 的矩阵
     */
    void init_cube_matrix()
    {
        // 各个 cube 的位置，范围是 [-1, 1]^3，最后会根据场景大小进行缩放


        uint32_t cube_dim      = 4u;    // 每个方向上 cube 的数量
        float    cube_interval = 2.f / (float) cube_dim;

        resource.cube_matrix.reserve(cube_dim * cube_dim * cube_dim);

        auto scale_matrix = glm::scale(glm::one<glm::mat4>(), glm::vec3(5.f));

        for (int i = 0; i < cube_dim; ++i)
        {
            for (int j = 0; j < cube_dim; ++j)
            {
                for (int k = 0; k < cube_dim; ++k)
                {
                    glm::vec3 pos          = glm::vec3(-1) + glm::vec3(i, j, k) * cube_interval;
                    auto      trans_matrix = glm::translate(glm::one<glm::mat4>(), pos * SCENE_RANGE);

                    resource.cube_matrix.push_back(trans_matrix * scale_matrix);
                }
            }
        }


        //        glm::vec3 pos          = {-0.4, 0.2, -0.3};
        //        auto      scale_matrix = glm::scale(glm::one<glm::mat4>(), glm::vec3(5.f));
        //        auto      trans_matrix = glm::translate(glm::one<glm::mat4>(), pos * SCENE_RANGE);
        //        resource.cube_matrix.push_back(trans_matrix * scale_matrix);


        //        glm::vec3 pos          = {0, 0, 0};
        //        auto      scale_matrix = glm::scale(glm::one<glm::mat4>(), glm::vec3(5.f));
        //        auto      trans_matrix = glm::translate(glm::one<glm::mat4>(), pos * SCENE_RANGE);
        //        resource.cube_matrix.push_back(trans_matrix * scale_matrix);


        //        Hiss::Rand rand;
        //
        //        for (auto& matrix: resource.cube_matrix)
        //        {
        //            // 每个立方体都放大，明显一点
        //            auto scale_mat = glm::scale(glm::one<glm::mat4>(), glm::vec3(5.f));
        //
        //            glm::vec3 pos = {rand.uniform_np1(), rand.uniform_np1(), rand.uniform_np1()};
        //
        //            auto trans_mat = glm::translate(glm::one<glm::mat4>(), pos * SCENE_RANGE * 0.5f);
        //
        //            matrix = trans_mat * scale_mat;
        //        }
    }
};

}    // namespace ForwardPlus


RUN(ForwardPlus::App)
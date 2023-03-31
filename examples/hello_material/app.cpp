#include "color_pass.hpp"
#include "utils/application.hpp"
#define HISS_CPP
#include "shader/common_type.glsl"
#include "shader/light_type.glsl"
#include "run.hpp"


namespace Material
{
class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {
        // 初始化各种资源
        for (auto& payload: payloads)
        {
            payload.depth_attach = Hiss::Image2D::create_depth_attach(engine.allocator, engine.device(),
                                                                      engine.depth_format(), engine.extent());
            payload.frame_ubo =
                    Hiss::Buffer::create_ubo_device(engine.device(), engine.allocator, sizeof(frame_data), "frame ubo");
        }

        init_light();
        init_scene_ubo();


        // 初始化 color pass
        std::vector<ColorPass::Resource> color_pass_resources{engine.frame_manager().frames_number()};
        for (int i = 0; i < engine.frame_manager().frames_number(); ++i)
        {
            color_pass_resources[i] = {
                    .depth_attach = payloads[i].depth_attach,
                    .frame_ubo    = payloads[i].frame_ubo,
                    .scene_ubo    = scene_ubo,
                    .light_ssbo   = light_ssbo,
            };
        }

        color_pass = std::make_unique<ColorPass>(engine, color_pass_resources);
    }

    std::unique_ptr<ColorPass> color_pass;

    bool need_update_lights = true;


    void update() override
    {
        auto& frame   = engine.current_frame();
        auto& payload = payloads[frame.frame_id()];

        update_frame_ubo(frame);

        if (need_update_lights)
        {
            update_lights(frame);
            need_update_lights = false;
        }

        {
            // color attach: layout trans
            Hiss::Frame::FrameCommandBuffer command_buffer(frame, "pre color pass");
            frame.image().memory_barrier(
                    {vk::PipelineStageFlagBits::eTopOfPipe}, {vk::PipelineStageFlagBits::eColorAttachmentOutput},
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, command_buffer());
        }

        color_pass->update(*viking.root_node);

        {
            // color attach: layout trans
            Hiss::Frame::FrameCommandBuffer command_buffer(frame, "post color pass", {frame.submit_semaphore()});
            frame.image().memory_barrier(
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                    {vk::PipelineStageFlagBits::eBottomOfPipe}, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR, command_buffer());
        }
    }


private:
    struct Payload
    {
        std::shared_ptr<Hiss::Image2D> depth_attach;
        std::shared_ptr<Hiss::Buffer>  frame_ubo;
    };

    std::vector<Payload> payloads{engine.frame_manager().frames_number()};

    std::vector<Shader::Light> lights;
    Shader::Frame              frame_data{};
    Shader::Scene              scene_data{};


    std::shared_ptr<Hiss::Buffer> light_ssbo;
    std::shared_ptr<Hiss::Buffer> scene_ubo;

    Hiss::MeshLoader viking{engine, model / "cube" / "cube.obj"};


    void init_light()
    {
        lights.resize(1);

        lights.front() = Shader::Light{
                .pos_world = {-5, 5, -5, 1.0},
                .color     = {0.3, 0.8, 0.8, 1.0},
                .range     = 20.f,
                .intensity = 1.f,
                .type      = POINT_LIGHT,
        };

        light_ssbo = Hiss::Buffer::create_ssbo(engine.device(), engine.allocator, sizeof(Shader::Light), "light ssbo");
        light_ssbo->mem_update(lights.data(), sizeof(Shader::Light) * lights.size());
    }

    void init_scene_ubo()
    {
        scene_data = Shader::Scene{
                .proj_matrix   = Hiss::perspective(60.f, engine.aspect(), 0.1f, 100.f),
                .screen_width  = engine.extent().width,
                .screen_height = engine.extent().height,
                .near          = 0.1f,
                .far           = 100.f,
                .light_num     = 1,
        };
        scene_data.in_proj_matrix = glm::inverse(scene_data.proj_matrix);

        scene_ubo =
                Hiss::Buffer::create_ubo_device(engine.device(), engine.allocator, sizeof(Shader::Scene), "scene ubo");
        scene_ubo->mem_update(&scene_data, sizeof(scene_data));
    }


    /**
     * TODO 这个似乎可以独立出去
     */
    void update_frame_ubo(Hiss::Frame& frame)
    {
        auto& payload = payloads[frame.frame_id()];

        frame_data = Shader::Frame{
                .view_matrix = glm::lookAtRH(glm::vec3{5.f, 5.f, 5.f}, glm::vec3{0.f}, glm::vec3{0.f, 1.f, 0.f}),
        };


        Hiss::Frame::FrameCommandBuffer command_buffer{frame, "update frame ubo"};
        payload.frame_ubo->mem_update(&frame_data, sizeof(frame_data));

        // 插入合适的 barrier
        payload.frame_ubo->memory_barrier(
                command_buffer(), {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite},
                {vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader,
                 vk::AccessFlagBits::eShaderRead});
    }


    void update_lights(Hiss::Frame& frame)
    {
        Hiss::Frame::FrameCommandBuffer command_buffer{frame, "update light"};

        for (auto& light: lights)
        {
            light.pos_view = frame_data.view_matrix * light.pos_world;
        }

        light_ssbo->memory_barrier(command_buffer(), {vk::PipelineStageFlagBits::eFragmentShader},
                                   {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite});
        command_buffer().updateBuffer(light_ssbo->vkbuffer(), 0, light_ssbo->size(), lights.data());
        light_ssbo->memory_barrier(command_buffer(),
                                   {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite},
                                   {vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead});
    }
};
}    // namespace Material


RUN(Material::App)
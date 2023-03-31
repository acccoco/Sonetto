#include "utils/application.hpp"
#include "engine/model2.hpp"
#include "bloom_pass.hpp"
#include "color_pass.hpp"
#include "combine_pass.hpp"
#include "run.hpp"

// TODO 插入场景模型
//  检查一下各种初始化
//  考虑不使用继承，而是使用各种工厂函数：且 shader_ptr


namespace Bloom
{
class App : public Hiss::IApplication
{
public:
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {}

    ColorPass   color_pass{engine};
    BloomPass   bloom_pass{engine};
    CombinePass combine_pass{engine};


    void prepare() override
    {
        create_color_image();
        for (auto&& payload: payloads)
        {
            payload.depth_attach = std::make_shared<Hiss::DepthAttach>(
                    engine.allocator, engine.device(), engine.depth_format(), engine.extent(), "depth attach");
        }


        // 初始化 color pass
        std::vector<ColorPass::Resource> color_resources{engine.frame_manager().frames_number()};
        for (int i = 0; i < engine.frame_manager().frames_number(); ++i)
        {
            color_resources[i] = {
                    .hdr_color     = payloads[i].hdr_color,
                    .depth_attach  = payloads[i].depth_attach,
                    .frame_uniform = payloads[i].frame_uniform,
                    .scene_uniform = scene_uniform,
                    .light_ssbo    = light_ssbo,
            };
        }
        color_pass.prepare(color_resources);


        // 初始化 bloom pass
        std::vector<BloomPass::Resource> bloom_resources{engine.frame_manager().frames_number()};
        for (int i = 0; i < engine.frame_manager().frames_number(); ++i)
        {
            bloom_resources[i] = {
                    .frag_color  = payloads[i].hdr_color,
                    .bloom_color = payloads[i].bloom_color,
            };
        }
        bloom_pass.prepare(bloom_resources);


        // 初始化 combine pass
        std::vector<CombinePass::Resource> combine_resources{engine.frame_manager().frames_number()};
        for (int i = 0; i < engine.frame_manager().frames_number(); ++i)
        {
            combine_resources[i] = {
                    .hdr_image    = payloads[i].hdr_color,
                    .bloom_image  = payloads[i].bloom_color,
                    .color_output = &engine.frame_manager().frame(i).image(),
            };
        }
    }

    void update() override
    {
        auto&& frame   = engine.current_frame();
        auto&& payload = payloads[frame.frame_id()];

        {
            Hiss::Frame::FrameCommandBuffer command_buffer(frame, "pre color pass");

            payload.hdr_color->memory_barrier(
                    {vk::PipelineStageFlagBits::eTopOfPipe},
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, command_buffer());
        }

        // TODO 插入 color pass

        {
            Hiss::Frame::FrameCommandBuffer command_buffer(frame, "pre bloom pass");

            // hdr 现在用于采样
            payload.hdr_color->memory_barrier(
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                    {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderRead},
                    vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                    command_buffer());

            // bloom color 用于存放颜色，转变为 storage image
            payload.bloom_color->memory_barrier(
                    {vk::PipelineStageFlagBits::eTopOfPipe},
                    {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite},
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, command_buffer());
        }

        bloom_pass.update();

        {
            Hiss::Frame::FrameCommandBuffer command_buffer(frame, "post bloom pass");

            // 需要在 bloom color 中采样
            payload.bloom_color->memory_barrier(
                    {vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite},
                    {vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead},
                    vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal, command_buffer());

            frame.image().memory_barrier(
                    {vk::PipelineStageFlagBits::eTopOfPipe},
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, command_buffer());
        }

        // TODO 插入 combine pass

        {
            Hiss::Frame::FrameCommandBuffer command_buffer(frame, "post combine pass", {frame.submit_semaphore()});

            // 转变 swapchain color，用于 present
            frame.image().memory_barrier(
                    {vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite},
                    {vk::PipelineStageFlagBits::eBottomOfPipe}, vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR, command_buffer());
        }
    }

    void clean() override
    {
        //
        bloom_pass.clean();
    }


private:
    struct Payload
    {
        std::shared_ptr<Hiss::Image2D>     hdr_color;    // 用于存放 hdr 场景渲染出的颜色
        std::shared_ptr<Hiss::DepthAttach> depth_attach;
        std::shared_ptr<Hiss::Image2D>     bloom_color;
        std::shared_ptr<Hiss::Buffer>      frame_uniform;
        Shader::Frame                      frame_ubo{};
    };
    std::vector<Payload>          payloads{engine.frame_manager().frames_number()};
    std::shared_ptr<Hiss::Buffer> scene_uniform;
    Shader::Scene                 scene_ubo{};
    std::vector<Shader::Light>    lights;
    std::shared_ptr<Hiss::Buffer> light_ssbo;


    // 场景中的模型
    Hiss::MeshLoader mesh_cube{engine, model / "cube" / "cube.obj"};

    void create_color_image()
    {
        for (auto&& payload: payloads)
        {
            payload.hdr_color = std::make_shared<Hiss::Image2D>(
                    engine.allocator, engine.device(),
                    Hiss::Image2DCreateInfo{
                            .name   = "hdr color image",
                            .format = vk::Format::eR16G16B16A16Sfloat,
                            .extent = engine.extent(),
                            .usage  = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                            .aspect = vk::ImageAspectFlagBits::eColor,
                    });
            payload.bloom_color = std::make_shared<Hiss::Image2D>(
                    engine.allocator, engine.device(),
                    Hiss::Image2DCreateInfo{
                            .name   = "bloom color image",
                            .format = vk::Format::eR16G16B16A16Sfloat,
                            .extent = engine.extent(),
                            .usage  = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                            .aspect = vk::ImageAspectFlagBits::eColor,
                    });
        }
    }


    /**
     * 初始化光源以及对应的 buffer
     */
    void create_light()
    {
        lights.resize(1);
        lights.front() = Shader::Light{
                .pos_world = {3, 4, 5, 0},
                .color     = {0.8, 0.7, 0.6, 1.0},
                .range     = 20.f,
                .intensity = 1.f,
                .type      = POINT_LIGHT,
        };
    }
};
}    // namespace Bloom


RUN(Bloom::App)
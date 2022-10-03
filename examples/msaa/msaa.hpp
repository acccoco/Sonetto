#pragma once
#include <array>
#include "engine.hpp"
#include "vk/pipeline.hpp"
#include "proj_config.hpp"
#include "vk/model.hpp"
#include "vk/framebuffer.hpp"
#include "vk/texture.hpp"
#include "utils/tools.hpp"


namespace MSAA
{

struct UniformBlock
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4(proj);
};


/**
 * MSAA 实现：
 * - framebuffer 需要有 3 个 attachment：color，depth，resolve
 * - color 和 depth attachment 都需要多个 sample
 */
class App : public Hiss::Engine
{
public:
    App()
        : Engine("msaa")
    {}
    ~App() override = default;


private:
    void prepare() override;
    void update(double delte_time) noexcept override;
    void resize() override;
    void clean() override;

    void render_pass_prepare();
    void render_pass_clean();

    void pipeline_prepare();
    void pipeline_clean();

    void msaa_framebuffer_prepare();
    void msaa_framebuffer_clean();

    void uniform_buffer_prepare();
    void uniform_buffer_clean();

    void descriptor_set_layout_prepare();
    void descriptor_set_prepare();
    void descriptor_clean();

    void command_record(vk::CommandBuffer command_buffer, uint32_t swapchain_image_index);
    void uniform_update();


    // members =======================================================


public:
    const std::string vert_shader_path = SHADER("msaa/msaa.vert.spv");
    const std::string frag_shader_path = SHADER("msaa/msaa.frag.spv");
    const std::string texture_path     = ASSETS("model/viking_room/tex/viking_room.png");
    const std::string model_path       = MODEL("viking_room/viking_room.obj");

    /* framebuffer attachment 的清除值，顺序要和 framebuffer 一致。不需要清除 resolve buffer */
    const std::__1::array<vk::ClearValue, 2> clear_values = {
            vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.2f, 0.2f, 0.2f, 1.f}}},
            vk::ClearValue{.depthStencil = {1.f, 0}},
    };

    vk::Viewport viewport = {
            .x        = 0.f,
            .y        = 0.f,
            .minDepth = 0.f,
            .maxDepth = 1.f,
    };


private:
    vk::RenderPass                      _msaa_renderpass       = VK_NULL_HANDLE;
    vk::Pipeline                        _pipeline              = VK_NULL_HANDLE;
    Hiss::PipelineTemplate              _pipeline_state        = {};
    vk::PipelineLayout                  _pipeline_layout       = VK_NULL_HANDLE;
    vk::DescriptorSetLayout             _descriptor_set_layout = VK_NULL_HANDLE;
    vk::DescriptorPool                  _descriptor_pool       = VK_NULL_HANDLE;
    std::__1::vector<vk::DescriptorSet> _descriptor_sets       = {};

    vk::SampleCountFlagBits _sample = vk::SampleCountFlagBits::e1;

    std::__1::array<Hiss::Buffer*, IN_FLIGHT_CNT> _uniform_buffers = {};

    Hiss::Image*                     _msaa_color_image  = nullptr;
    Hiss::Image*                     _msaa_depth_image  = nullptr;
    Hiss::ImageView*                 _msaa_color_view   = nullptr;
    Hiss::ImageView*                 _msaa_depth_view   = nullptr;
    std::vector<Hiss::Framebuffer3*> _msaa_framebuffers = {};

    Hiss::Mesh*    _mesh    = nullptr;
    Hiss::Texture* _texture = nullptr;
};
}    // namespace MSAA

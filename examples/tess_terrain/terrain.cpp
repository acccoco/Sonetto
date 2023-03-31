#include "engine/engine.hpp"
#include "utils/application.hpp"
#include "utils/tools.hpp"
#include "proj_config.hpp"
#include "engine/vertex.hpp"
#include "utils/pipeline_template.hpp"
#include "engine/texture.hpp"
#include "engine/vertex_buffer.hpp"
#include "utils/stbi.hpp"
#include "run.hpp"


/**
 * 利用 tesselation 生成地形
 * tessellation 的配置：
 *  1. pipeline 创建时，指定 assemly 为 patch，并在 tessellation state 中指定 patch 中控制点的数量
 *  2. 传递 tsc（可选） 和 tse 着色器
 */
namespace Terrain
{

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv;

    static std::vector<vk::VertexInputBindingDescription> binding_description(uint32_t binding)
    {
        return {{binding, sizeof(Vertex), vk::VertexInputRate::eVertex}};
    }

    static std::vector<vk::VertexInputAttributeDescription> attribute_description(uint32_t binding)
    {
        return {{0, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
                {1, binding, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)}};
    }
};


// 用于 tessellation evaluation shader 的 uniform object
struct TeseUBO
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};


// 每一帧需要用 fence 保护的资源
struct FramePayload
{
    vk::CommandBuffer command_buffer;
};


class App : public Hiss::IApplication
{
public:
#pragma region 配置信息

    const uint32_t grid_res = 20;    // 粗糙网格的分辨率

    const std::vector<vk::DescriptorSetLayoutBinding> descriptor_bindings = {
            {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eTessellationEvaluation},
            {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eTessellationEvaluation},
    };

    vk::RenderingAttachmentInfo color_attach_info = {
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearValue{.color = {.float32 = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.f}}},
    };

    vk::RenderingAttachmentInfo depth_attach_info = {
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eDontCare,
            .clearValue  = vk::ClearValue{.depthStencil = {1.f, 0}},
    };

    vk::RenderingInfo rendering_info = {
            .renderArea           = {.offset = {0, 0}},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &color_attach_info,
            .pDepthAttachment     = &depth_attach_info,
    };

    vk::Viewport viewport{.minDepth = 0.f, .maxDepth = 1.f};

#pragma endregion


#pragma region 外部资源

    const std::filesystem::path height_map  = texture / "tess_terrain/iceland_heightmap.png";
    const std::filesystem::path vert_shader = shader / "tess_terrain/terrain.vert";
    const std::filesystem::path frag_shader = shader / "tess_terrain/terrain.frag";
    const std::filesystem::path tesc_shader = shader / "tess_terrain/terrain.tesc";
    const std::filesystem::path tese_shader = shader / "tess_terrain/terrain.tese";

#pragma endregion


    TeseUBO                      ubo{};
    Hiss::Texture*               tex_height{};
    vk::DescriptorSet            descriptor_set;
    Hiss::UniformBuffer*         uniform_buffer{};
    Hiss::VertexBuffer2<Vertex>* vertex_buffer = nullptr;
    Hiss::Image2D*               depth_buffer{};

    vk::DescriptorSetLayout descriptor_set_layout;
    vk::PipelineLayout      pipeline_layout;
    struct
    {
        vk::Pipeline terrain;
        vk::Pipeline wireframe;

        vk::Pipeline current;
    } pipelines;


    std::vector<FramePayload> payloads;


#pragma region 特殊接口
    explicit App(Hiss::Engine& engine)
        : Hiss::IApplication(engine)
    {}
    ~App() override = default;


    void prepare() override
    {
        create_pipeline();

        // 是否以线框模式渲染
        pipelines.current = pipelines.wireframe;

        depth_buffer = engine.create_depth_attach(vk::SampleCountFlagBits::e1);
        load_vertex_data();
        create_descriptor_set();

        payloads.resize(engine.frame_manager().frames_number());
        for (auto& payload: payloads)
            payload.command_buffer = engine.device().command_pool().command_buffer_create().front();
    }

    void resize() override {}

    void update() override
    {
        auto& frame   = engine.current_frame();
        auto& payload = payloads[frame.frame_id()];

        payload.command_buffer.reset();
        record_command(payload.command_buffer, frame);

        engine.queue().submit_commands({}, {payload.command_buffer}, {frame.submit_semaphore()}, frame.insert_fence());
    }

    void clean() override
    {
        delete depth_buffer;
        delete tex_height;
        delete vertex_buffer;
        delete uniform_buffer;

        engine.vkdevice().destroy(pipelines.terrain);
        engine.vkdevice().destroy(pipelines.wireframe);
        engine.vkdevice().destroy(pipeline_layout);
        engine.vkdevice().destroy(descriptor_set_layout);
    }

#pragma endregion


    void create_pipeline()
    {
        // descriptor set layout
        vk::DescriptorSetLayoutCreateInfo descriptor_info;
        descriptor_set_layout =
                engine.vkdevice().createDescriptorSetLayout(descriptor_info.setBindings(descriptor_bindings));


        // pipeline layout
        pipeline_layout = engine.vkdevice().createPipelineLayout({
                .setLayoutCount = 1,
                .pSetLayouts    = &descriptor_set_layout,
        });


        // pipeline
        Hiss::PipelineTemplate pipeline_template = {
                .shader_stages        = {engine.shader_loader().load(vert_shader, vk::ShaderStageFlagBits::eVertex),
                                         engine.shader_loader().load(frag_shader, vk::ShaderStageFlagBits::eFragment),
                                         engine.shader_loader().load(tesc_shader,
                                                                     vk::ShaderStageFlagBits::eTessellationControl),
                                         engine.shader_loader().load(tese_shader,
                                                                     vk::ShaderStageFlagBits::eTessellationEvaluation)},
                .vertex_bindings      = Vertex::binding_description(0),
                .vertex_attributes    = Vertex::attribute_description(0),
                .color_attach_formats = {engine.color_format()},
                .depth_attach_format  = engine.depth_format(),
                .pipeline_layout      = pipeline_layout,
                .dynamic_states       = {vk::DynamicState::eViewport, vk::DynamicState::eScissor},
                .tessellation_state   = vk::PipelineTessellationStateCreateInfo{.patchControlPoints = 4},
        };
        pipeline_template.assembly_state.topology = vk::PrimitiveTopology::ePatchList;


        // terrain pipeline
        pipelines.terrain = pipeline_template.generate(engine.device());


        // wireframe pipeline
        pipeline_template._rasterization_state.polygonMode = vk::PolygonMode::eLine;
        pipelines.wireframe                                = pipeline_template.generate(engine.device());
    }


    // 读取顶点数据
    void load_vertex_data()
    {
        // 读取高度图的纹理
        Hiss::Stbi_8Bit_RAII image(height_map);
        spdlog::info("height map resolution: ({}, {}), channels: {}", image.width(), image.height(),
                     image.channels_in_file());


        // 整个网格的起点和间隔
        float x_min    = -(float) image.width() / 2.f;
        float z_min    = -(float) image.height() / 2.f;
        float delta_x  = (float) image.width() / (float) grid_res;
        float delta_z  = (float) image.height() / (float) grid_res;
        float delta_uv = 1.f / (float) grid_res;


        // 生成顶点数据
        // 网格的方向以及顶点顺序（顶点顺序没有特殊要求，以什么顺序输入，在 tess shader 中就以什么顺序访问
        //     ---------> x, u
        //     | 0    1
        //     |
        //     | 2    3
        //     v z, v
        std::vector<Vertex> vertices;
        for (uint32_t i = 0; i < grid_res; ++i)
        {
            for (uint32_t j = 0; j < grid_res; ++j)
            {
                // 当前网格的起点
                glm::vec3 pos_start = {x_min + delta_x * (float) i, 0, z_min + delta_z * (float) j};
                glm::vec2 uv_start  = {(float) i * delta_uv, (float) j * delta_uv};

                // 用于镶嵌的 patch 是四边形
                vertices.push_back(Vertex{
                        pos_start,
                        uv_start,
                });
                vertices.push_back(Vertex{
                        pos_start + glm::vec3{delta_x, 0, 0},
                        uv_start + glm::vec2{delta_uv, 0},
                });
                vertices.push_back(Vertex{
                        pos_start + glm::vec3{0, 0, delta_z},
                        uv_start + glm::vec2{0, delta_uv},
                });
                vertices.push_back(Vertex{
                        pos_start + glm::vec3{delta_x, 0, delta_z},
                        uv_start + glm::vec2{delta_uv, delta_uv},
                });
            }
        }


        // 创建 buffer
        vertex_buffer = new Hiss::VertexBuffer2<Vertex>(engine.device(), engine.allocator, vertices);
    }


    void record_command(vk::CommandBuffer command_buffer, Hiss::Frame& frame)
    {
        command_buffer.begin(vk::CommandBufferBeginInfo{});

        // 内存屏障与布局转换
        Hiss::Engine::depth_attach_execution_barrier(command_buffer, *depth_buffer);

        Hiss::Engine::color_attach_layout_trans_1(command_buffer, frame.image());

        // framebuffer 相关设置
        color_attach_info.imageView      = frame.image().vkview();
        depth_attach_info.imageView      = depth_buffer->vkview();
        rendering_info.renderArea.extent = engine.extent();

        command_buffer.beginRendering(rendering_info);
        {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.current);
            command_buffer.setViewport(
                    0, viewport.setWidth((float) engine.extent().width).setHeight((float) engine.extent().height));
            command_buffer.setScissor(0, vk::Rect2D{.extent = engine.extent()});


            command_buffer.bindVertexBuffers(0, {vertex_buffer->vkbuffer()}, {0});
            command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, {descriptor_set},
                                              {});
            command_buffer.draw(vertex_buffer->vertex_num, 1, 0, 0);
        }
        command_buffer.endRendering();

        Hiss::Engine::color_attach_layout_trans_2(command_buffer, frame.image());

        command_buffer.end();
    }


    // 创建 uniform buffer，texture，descriptor set，并与 descriptor set 绑定
    void create_descriptor_set()
    {
        // 读取纹理
        tex_height = new Hiss::Texture(engine.device(), engine.allocator, height_map, vk::Format::eR8G8B8A8Unorm);


        // 设置初始的 ubo
        ubo = {
                .model = glm::one<glm::mat4>(),
                .view  = glm::lookAt(glm::vec3(-200.f, 200.f, 200.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f)),
                .projection =
                        glm::perspective(glm::radians(60.f),
                                         (float) engine.extent().width / (float) engine.extent().height, 0.1f, 1024.f),
        };
        ubo.projection[1][1] *= -1.f;
        uniform_buffer = new Hiss::UniformBuffer(engine.device(), engine.allocator, sizeof(TeseUBO), "");
        uniform_buffer->mem_copy(&ubo, sizeof(ubo));


        // 创建 descriptor set
        descriptor_set = engine.vkdevice()
                                 .allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
                                         .descriptorPool     = engine.descriptor_pool(),
                                         .descriptorSetCount = 1,
                                         .pSetLayouts        = &descriptor_set_layout,
                                 })
                                 .front();


        // 将 buffer 和 descriptor set 绑定起来
        vk::DescriptorImageInfo image_info = {.sampler     = tex_height->sampler(),
                                              .imageView   = tex_height->image().vkview(),
                                              .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

        vk::DescriptorBufferInfo buffer_info = {.buffer = uniform_buffer->vkbuffer(),
                                                .offset = 0,
                                                .range  = VK_WHOLE_SIZE};

        engine.vkdevice().updateDescriptorSets(
                {
                        vk::WriteDescriptorSet{.dstSet          = descriptor_set,
                                               .dstBinding      = 0,
                                               .descriptorCount = 1,
                                               .descriptorType  = vk::DescriptorType::eUniformBuffer,
                                               .pBufferInfo     = &buffer_info},
                        vk::WriteDescriptorSet{.dstSet          = descriptor_set,
                                               .dstBinding      = 1,
                                               .descriptorCount = 1,
                                               .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                                               .pImageInfo      = &image_info},
                },
                {});
    }

private:
};
}    // namespace Terrain


RUN(Terrain::App)
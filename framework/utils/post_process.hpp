#pragma once
#include "engine/engine.hpp"
#include "engine/vertex_buffer.hpp"

/**
 * post process 相关的资源
 */

namespace Hiss
{
struct PostProcess
{
    using Vertex       = glm::vec2;
    using VertexBuffer = VertexBuffer2<Vertex>;


    /**
     * 顶点数据，正面为 CCW
     * 绘制时不需要 index buffer
     */
    inline static const std::vector<Vertex> vertices = {
            {1, 0}, {0, 0}, {0, 1},    // 第一个三角形
            {1, 0}, {0, 1}, {1, 1},    // 第二个三角形
    };

    inline static const std::vector<vk::VertexInputBindingDescription> binding_descriptions = {
            vk::VertexInputBindingDescription{
                    .binding   = 0,
                    .stride    = sizeof(PostProcess::Vertex),
                    .inputRate = vk::VertexInputRate::eVertex,
            },
    };


    inline static const std::vector<vk::VertexInputAttributeDescription> attribute_descriptions = {
            vk::VertexInputAttributeDescription{
                    .location = 0,
                    .binding  = 0,
                    .format   = vk::Format::eR32G32Sfloat,
                    .offset   = 0,
            },
    };


    static std::shared_ptr<VertexBuffer> vertex_buffer(Hiss::Engine& engine)
    {
        return std::make_shared<VertexBuffer>(engine.device(), engine.allocator, vertices, "post process vertices");
    }
};
}    // namespace Hiss
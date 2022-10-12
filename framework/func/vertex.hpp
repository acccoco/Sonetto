#pragma once
#include "core/device.hpp"
#include "buffer.hpp"


namespace Hiss
{

// 顶点格式 (location 0) vec2 position, (location 1) vec3 color
struct Vertex2DColor
{
    glm::vec2 position;
    glm::vec3 color;


    static std::vector<vk::VertexInputBindingDescription> get_binding_description(uint32_t bindind)
    {
        return {
                vk::VertexInputBindingDescription{
                        // 表示一个 vertex buffer，一次渲染的顶点数据可能位于多个 vertex buffer 中
                        .binding   = bindind,
                        .stride    = sizeof(Hiss::Vertex2DColor),
                        .inputRate = vk::VertexInputRate::eVertex,
                },
        };
    }


    static std::vector<vk::VertexInputAttributeDescription> get_attribute_description(uint32_t binding)
    {
        return {
                vk::VertexInputAttributeDescription{
                        .location = 0,
                        .binding  = binding,
                        .format   = vk::Format::eR32G32Sfloat,    // signed float
                        .offset   = offsetof(Vertex2DColor, position),
                },
                vk::VertexInputAttributeDescription{
                        .location = 1,
                        .binding  = binding,
                        .format   = vk::Format::eR32G32B32Sfloat,
                        .offset   = offsetof(Vertex2DColor, color),
                },
        };
    }
};


struct Vertex3DColorUv
{
    glm::vec3 pos;
    glm::vec2 tex_coord;
    glm::vec3 color;


    bool operator==(const Hiss::Vertex3DColorUv& other) const;

    static std::vector<vk::VertexInputBindingDescription>   binding_description_get(uint32_t binding);
    static std::vector<vk::VertexInputAttributeDescription> attribute_description_get(uint32_t binding);
};

}    // namespace Hiss


namespace std
{


// template speciliation 为了让 Vertex3DColor 类型支持 hash 运算
template<>
struct hash<Hiss::Vertex3DColorUv>
{
    size_t operator()(Hiss::Vertex3DColorUv const& vertex) const
    {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1)
             ^ (hash<glm::vec2>()(vertex.tex_coord) << 1);
    }
};
}    // namespace std

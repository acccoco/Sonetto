#pragma once
#include "vk/device.hpp"
#include "vk/buffer.hpp"


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

    static std::array<vk::VertexInputBindingDescription, 1>   binding_description_get(uint32_t binding);
    static std::array<vk::VertexInputAttributeDescription, 3> attribute_description_get(uint32_t binding);
};

}    // namespace Hiss


namespace std
{
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


namespace Hiss
{

template<typename vertex_t>
class VertexBuffer : public Buffer
{

public:
    using element_t = vertex_t;

    VertexBuffer(const Device& device, const std::vector<vertex_t>& vertices)
        : Buffer(device, sizeof(vertex_t) * vertices.size(),
                 vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                 vk::MemoryPropertyFlagBits::eDeviceLocal),
          _vertex_cnt(vertices.size())
    {
        /* vertex data to stage buffer */
        Hiss::Buffer stage_buffer{device, buffer_size(), vk::BufferUsageFlagBits::eTransferSrc,
                                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
        stage_buffer.memory_copy_in(reinterpret_cast<const void*>(vertices.data()), static_cast<size_t>(buffer_size()));


        /* stage buffer to vertex buffer */
        OneTimeCommand command_buffer{device, device.command_pool()};
        command_buffer().copyBuffer(stage_buffer.vkbuffer(), _buffer, {vk::BufferCopy{.size = buffer_size()}});
        command_buffer.exec();
    }

    ~VertexBuffer() override = default;


private:
    size_t _vertex_cnt = 0;
};


class IndexBuffer : public Buffer
{
public:
    using element_t = uint32_t;

    IndexBuffer(Device& device, const std::vector<element_t>& indices);
    ~IndexBuffer() override = default;


private:
    size_t _index_cnt = 0;
};


class IndexBuffer2 : public Buffer2
{
public:
    // memory flags 表示：DEVICE_LOCAL
    IndexBuffer2(Device& device, VmaAllocator allocator, const std::vector<uint32_t>& indices)
        : Buffer2(allocator, sizeof(uint32_t) * indices.size(),
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT),
          index_num(indices.size())
    {
        // 创建 stage buffer，向其中写入数据
        StageBuffer stage_buffer(allocator, size());
        stage_buffer.mem_copy(indices.data(), size());


        // 立即向 indices 中写入
        OneTimeCommand command_buffer{device, device.command_pool()};
        command_buffer().copyBuffer(stage_buffer.vkbuffer(), vkbuffer(), {vk::BufferCopy{.size = size()}});
        command_buffer.exec();
    }


public:
    const size_t index_num;
};


template<typename VertexType>
class VertexBuffer2 : public Buffer2
{
public:
    // memory flags: DEVICE_LOCAL
    VertexBuffer2(Device& device, VmaAllocator allocator, const std::vector<VertexType>& vertices)
        : Buffer2(allocator, sizeof(VertexType) * vertices.size(),
                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT),
          vertex_num(vertices.size())
    {
        // 创建 stage buffer，向其中写入数据
        StageBuffer stage_buffer(allocator, size());
        stage_buffer.mem_copy(vertices.data(), size());


        // 立即向 vertices bufffer 中写入
        OneTimeCommand command_buffer{device, device.command_pool()};
        command_buffer().copyBuffer(stage_buffer.vkbuffer(), vkbuffer(), {vk::BufferCopy{.size = size()}});
        command_buffer.exec();
    }


public:
    const size_t vertex_num;
};

}    // namespace Hiss
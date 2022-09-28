#pragma once
#include "vk/device.hpp"
#include "vk/buffer.hpp"


namespace Hiss
{
struct Vertex2DColor
{
    glm::vec2 position;
    glm::vec3 color;

    static std::array<vk::VertexInputBindingDescription, 1>   binding_description_get(uint32_t bindind);
    static std::array<vk::VertexInputAttributeDescription, 2> attribute_description_get(uint32_t binding);
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

    VertexBuffer(Device& device, const std::vector<vertex_t>& vertices)
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


class IndexBuffer2 : public DeviceBuffer
{
public:
    IndexBuffer2(Device& device, VmaAllocator allocator, const std::vector<uint32_t>& indices)
        : DeviceBuffer(allocator, sizeof(uint32_t) * indices.size(),
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
          index_num(indices.size())
    {
        // 创建 stage buffer，向其中写入数据
        StageBuffer stage_buffer(allocator, size());
        stage_buffer.mem_copy(indices.data(), size());


        // 立即向 indices 中写入
        OneTimeCommand command_buffer{device, device.command_pool()};
        command_buffer().copyBuffer(stage_buffer.buffer(), buffer(), {vk::BufferCopy{.size = size()}});
        command_buffer.exec();
    }


public:
    const size_t index_num;
};


template<typename VertexType>
class VertexBuffer2 : public DeviceBuffer
{
public:
    VertexBuffer2(Device& device, VmaAllocator allocator, const std::vector<VertexType>& vertices)
        : DeviceBuffer(allocator, sizeof(VertexType) * vertices.size(),
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
          vertex_num(vertices.size())
    {
        // 创建 stage buffer，向其中写入数据
        StageBuffer stage_buffer(allocator, size());
        stage_buffer.mem_copy(vertices.data(), size());


        // 立即向 vertices bufffer 中写入
        OneTimeCommand command_buffer{device, device.command_pool()};
        command_buffer().copyBuffer(stage_buffer.buffer(), buffer(), {vk::BufferCopy{.size = size()}});
        command_buffer.exec();
    }


public:
    const size_t vertex_num;
};

}    // namespace Hiss
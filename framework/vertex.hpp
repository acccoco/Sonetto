#pragma once
#include "device.hpp"
#include "buffer.hpp"


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
        Hiss::Buffer stage_buffer{device, _buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                                  vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
        stage_buffer.memory_copy_in(reinterpret_cast<const void*>(vertices.data()), static_cast<size_t>(_buffer_size));


        /* stage buffer to vertex buffer */
        OneTimeCommand command_buffer{device, device.command_pool_graphics()};
        command_buffer().copyBuffer(stage_buffer.vkbuffer(), _buffer, {vk::BufferCopy{.size = _buffer_size}});
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

}    // namespace Hiss
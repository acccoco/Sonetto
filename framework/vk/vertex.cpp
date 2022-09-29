#include "vk/vertex.hpp"
#include "vk/buffer.hpp"


Hiss::IndexBuffer::IndexBuffer(Hiss::Device& device, const std::vector<element_t>& indices)
    : Buffer(device, sizeof(element_t) * indices.size(),
             vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
             vk::MemoryPropertyFlagBits::eDeviceLocal),
      _index_cnt(indices.size())
{
    /* index data to stage buffer */
    Hiss::Buffer stage_buffer{device, buffer_size(), vk::BufferUsageFlagBits::eTransferSrc,
                              vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
    stage_buffer.memory_copy_in(reinterpret_cast<const void*>(indices.data()), static_cast<size_t>(buffer_size()));


    /* stage buffer to vertex buffer */
    OneTimeCommand command_buffer{device, device.command_pool()};
    command_buffer().copyBuffer(stage_buffer.vkbuffer(), _buffer, {vk::BufferCopy{.size = buffer_size()}});
    command_buffer.exec();
}


std::array<vk::VertexInputBindingDescription, 1> Hiss::Vertex3DColorUv::binding_description_get(uint32_t binding)
{
    return {
            vk::VertexInputBindingDescription{
                    .binding   = binding,
                    .stride    = sizeof(Hiss::Vertex3DColorUv),
                    .inputRate = vk::VertexInputRate::eVertex,
            },
    };
}


std::array<vk::VertexInputAttributeDescription, 3> Hiss::Vertex3DColorUv::attribute_description_get(uint32_t binding)
{
    return {
            vk::VertexInputAttributeDescription{
                    .location = 0,
                    .binding  = binding,
                    .format   = vk::Format::eR32G32B32Sfloat,
                    .offset   = offsetof(Hiss::Vertex3DColorUv, pos),
            },
            vk::VertexInputAttributeDescription{
                    .location = 1,
                    .binding  = binding,
                    .format   = vk::Format::eR32G32B32Sfloat,
                    .offset   = offsetof(Hiss::Vertex3DColorUv, color),
            },
            vk::VertexInputAttributeDescription{
                    .location = 2,
                    .binding  = binding,
                    .format   = vk::Format::eR32G32Sfloat,
                    .offset   = offsetof(Hiss::Vertex3DColorUv, tex_coord),
            },
    };
}


bool Hiss::Vertex3DColorUv::operator==(const Hiss::Vertex3DColorUv& other) const
{
    return pos == other.pos && tex_coord == other.tex_coord && color == other.color;
}
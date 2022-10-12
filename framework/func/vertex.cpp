#include "vertex.hpp"
#include "buffer.hpp"


std::vector<vk::VertexInputBindingDescription> Hiss::Vertex3DColorUv::binding_description_get(uint32_t binding)
{
    return {
            vk::VertexInputBindingDescription{
                    .binding   = binding,
                    .stride    = sizeof(Hiss::Vertex3DColorUv),
                    .inputRate = vk::VertexInputRate::eVertex,
            },
    };
}


std::vector<vk::VertexInputAttributeDescription> Hiss::Vertex3DColorUv::attribute_description_get(uint32_t binding)
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
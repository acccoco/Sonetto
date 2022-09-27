#pragma once

#include "proj_profile.hpp"
#include "vk/vk_common.hpp"
#include "vertex.hpp"
#include "tiny_obj_loader.h"
#include <unordered_map>


namespace Hiss
{
class Mesh    // TODO 可以考虑添加 template
{
public:
    Mesh(Device& device, std::string mesh_path);
    ~Mesh();

    [[nodiscard]] size_t                                     vertex_cnt() const { return _vertex_num; }
    [[nodiscard]] size_t                                     index_cnt() const { return _index_num; }
    [[nodiscard]] Hiss::VertexBuffer<Hiss::Vertex3DColorUv>& vertex_buffer() const { return *_vertex_buffer; }
    [[nodiscard]] Hiss::IndexBuffer&                         index_buffer() const { return *_index_buffer; }


private:
    void tiny_obj_load(std::vector<Hiss::Vertex3DColorUv>&        vertices,
                       std::vector<Hiss::IndexBuffer::element_t>& indices);


    // members =======================================================

private:
    Device&                                    _device;
    const std::string                          _mesh_path;
    Hiss::VertexBuffer<Hiss::Vertex3DColorUv>* _vertex_buffer = nullptr;
    Hiss::IndexBuffer*                         _index_buffer  = nullptr;
    size_t                                     _vertex_num    = {};
    size_t                                     _index_num     = {};
};


}    // namespace Hiss
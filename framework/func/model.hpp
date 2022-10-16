#pragma once

#include "proj_config.hpp"
#include "core/vk_common.hpp"
#include "vertex.hpp"
#include "tiny_obj_loader.h"
#include <unordered_map>
#include "core/engine.hpp"
#include "vertex_buffer.hpp"


namespace Hiss
{
class Mesh
{
public:
    Mesh(Hiss::Engine& engine, const std::string& mesh_path, const std::string& name = "");
    ~Mesh();

    Hiss::VertexBuffer2<Hiss::Vertex3DNormalUV>& vertex_buffer() { return *_vertex_buffer2; }
    Hiss::IndexBuffer2&                          index_buffer() { return *_index_buffer2; }

    Prop<std::string, Mesh> mesh_path;


private:
    void tiny_obj_load(std::vector<Hiss::Vertex3DNormalUV>&        vertices,
                       std::vector<Hiss::IndexBuffer2::element_t>& indices) const;


    // members =======================================================

private:
    Hiss::Engine& engine;

    Hiss::VertexBuffer2<Hiss::Vertex3DNormalUV>* _vertex_buffer2;
    Hiss::IndexBuffer2*                          _index_buffer2;
};


}    // namespace Hiss
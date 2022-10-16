#include "model.hpp"

#include <utility>
#include <fmt/format.h>
#include "utils/tools.hpp"
#include "core/engine.hpp"


Hiss::Mesh::Mesh(Hiss::Engine& engine, const std::string& mesh_path, const std::string& name)
    : mesh_path(mesh_path),
      engine(engine)
{
    // 从 .obj 文件中读取 vertices 以及 indices
    std::vector<Hiss::Vertex3DNormalUV>        vertices;
    std::vector<Hiss::IndexBuffer2::element_t> indices;
    tiny_obj_load(vertices, indices);


    // 创建 vertex buffer 以及 index buffer
    _vertex_buffer2 = new Hiss::VertexBuffer2<Hiss::Vertex3DNormalUV>(engine.device(), engine.allocator, vertices,
                                                                      fmt::format("{}-vertex-buffer", name));
    _index_buffer2 =
            new Hiss::IndexBuffer2(engine.device(), engine.allocator, indices, fmt::format("{}-index-buffer", name));
}


Hiss::Mesh::~Mesh()
{
    delete _vertex_buffer2;
    delete _index_buffer2;
}


void Hiss::Mesh::tiny_obj_load(std::vector<Hiss::Vertex3DNormalUV>&        vertices,
                               std::vector<Hiss::IndexBuffer2::element_t>& indices) const
{
    tinyobj::attrib_t attr;                  // 整个模型文件的顶点属性，包括 pos, normal, uv
    std::vector<tinyobj::shape_t> shapes;    // 每个 shape 都是一个模型，里面主要是各个 face 的 indices 数据
    std::vector<tinyobj::material_t> materials;


    // 读取 obj 文件，会自动三角化
    std::string err_msg;
    if (!tinyobj::LoadObj(&attr, &shapes, &materials, &err_msg, mesh_path().c_str(), nullptr, true))
        throw std::runtime_error(fmt::format("fail to load obj file: ({}), error message: ({})", mesh_path(), err_msg));


    for (const auto& shape: shapes)
    {
        for (const auto& index: shape.mesh.indices)
        {
            Vertex3DNormalUV vertex = {
                    .pos = {attr.vertices[3 * index.vertex_index + 0], attr.vertices[3 * index.vertex_index + 1],
                            attr.vertices[3 * index.vertex_index + 2]},

                    .normal = {attr.normals[3 * index.normal_index+ 0], attr.normals[3 * index.normal_index + 1],
                               attr.normals[3 * index.normal_index + 2]},

                    /**
                     * stbi 将 picture 左上角视为 data[0]，
                     * vulkan 将 data[0] 视为 texture 的左上角，
                     * .obj 文件将 texture 左下角视为 uv 的起点，因此 tex coord 的 v 需要做如下变换
                     */
                    .uv = {attr.texcoords[2 * index.texcoord_index + 0],
                           1.f - attr.texcoords[2 * index.texcoord_index + 1]},
            };


            indices.push_back(vertices.size());
            vertices.push_back(vertex);
        }
    }
}

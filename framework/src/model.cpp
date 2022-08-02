#include "model.hpp"

#include <utility>
#include "tools.hpp"


Hiss::Mesh::Mesh(Hiss::Device& device, std::string mesh_path)
    : _device(device),
      _mesh_path(std::move(mesh_path))
{
    std::vector<Hiss::Vertex3DColorUv>        vertices;
    std::vector<Hiss::IndexBuffer::element_t> indices;
    tiny_obj_load(vertices, indices);

    _vertex_num = vertices.size();
    _index_num  = indices.size();

    _vertex_buffer = new Hiss::VertexBuffer<Hiss::Vertex3DColorUv>(_device, vertices);
    _index_buffer  = new IndexBuffer(_device, indices);
}


Hiss::Mesh::~Mesh()
{
    DELETE(_vertex_buffer);
    DELETE(_index_buffer);
}


void Hiss::Mesh::tiny_obj_load(std::vector<Hiss::Vertex3DColorUv>&        vertices,
                               std::vector<Hiss::IndexBuffer::element_t>& indices)
{
    /**
     * TinyObjLoader 不会重复利用模型中的顶点，这里手动重用.
     * Vertex hash 需要实现两个函数：equality test, hash calculation
     */
    std::unordered_map<Vertex3DColorUv, uint32_t> uniq_vertices;

    tinyobj::attrib_t                attr;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err_msg;
    if (!tinyobj::LoadObj(&attr, &shapes, &materials, &err_msg, _mesh_path.c_str()))
        throw std::runtime_error("error on load obj file" + _mesh_path + ", err: " + err_msg);

    for (const auto& shape: shapes)
    {
        for (const auto& index: shape.mesh.indices)
        {
            Vertex3DColorUv vertex = {
                    .pos = {attr.vertices[3 * index.vertex_index + 0], attr.vertices[3 * index.vertex_index + 1],
                            attr.vertices[3 * index.vertex_index + 2]},

                    /**
                     * stbi 将 picture 左上角视为 data[0]，
                     * vulkan 将 data[0] 视为 texture 的左上角，
                     * .obj 文件将 picture 左下角视为 uv 的起点，因此 tex coord 的 v 需要做如下变换
                     */
                    .tex_coord = {attr.texcoords[2 * index.texcoord_index + 0],
                                  1.f - attr.texcoords[2 * index.texcoord_index + 1]},
                    .color     = {0.5f, 0.5f, 0.5f},
            };

            if (uniq_vertices.count(vertex) == 0)
            {
                uniq_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniq_vertices[vertex]);
        }
    }
}

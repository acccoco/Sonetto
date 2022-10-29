#pragma once
#include <memory>
#include <filesystem>

#include <fmt/format.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "core/vk_include.hpp"
#include "engine/engine.hpp"
#include "engine/vertex.hpp"
#include "engine/texture.hpp"
#include "utils/vk_func.hpp"
#include "material.hpp"


namespace Hiss
{


struct Vertex3D
{
    glm::vec3 position;     // location 0
    glm::vec3 normal;       // location 1
    glm::vec3 tangent;      // location 2
    glm::vec3 bitangent;    // location 3
    glm::vec2 uv;           // location 4

    static std::vector<vk::VertexInputBindingDescription> binding_description(uint32_t bindind)
    {
        return {
                vk::VertexInputBindingDescription{
                        // 表示一个 vertex buffer，一次渲染的顶点数据可能位于多个 vertex buffer 中
                        .binding   = bindind,
                        .stride    = sizeof(Hiss::Vertex3D),
                        .inputRate = vk::VertexInputRate::eVertex,
                },
        };
    }


    static std::vector<vk::VertexInputAttributeDescription> attribute_description(uint32_t binding)
    {
        return {
                vk::VertexInputAttributeDescription{
                        .location = 0,
                        .binding  = binding,
                        .format   = vk::Format::eR32G32B32Sfloat,    // signed float
                        .offset   = offsetof(Vertex3D, position),
                },
                vk::VertexInputAttributeDescription{
                        .location = 1,
                        .binding  = binding,
                        .format   = vk::Format::eR32G32B32Sfloat,
                        .offset   = offsetof(Vertex3D, normal),
                },
                vk::VertexInputAttributeDescription{
                        .location = 2,
                        .binding  = binding,
                        .format   = vk::Format::eR32G32B32Sfloat,
                        .offset   = offsetof(Vertex3D, tangent),
                },
                vk::VertexInputAttributeDescription{
                        .location = 3,
                        .binding  = binding,
                        .format   = vk::Format::eR32G32B32Sfloat,
                        .offset   = offsetof(Vertex3D, bitangent),
                },
                vk::VertexInputAttributeDescription{
                        .location = 4,
                        .binding  = binding,
                        .format   = vk::Format::eR32G32Sfloat,
                        .offset   = offsetof(Vertex3D, uv),
                },
        };
    }
};


/**
 * 几何信息
 */
struct Mesh2
{
    std::vector<Vertex3D>     vertices;
    std::vector<FaceTriangle> faces;

    std::unique_ptr<Hiss::VertexBuffer2<Vertex3D>> vertex_buffer;
    std::unique_ptr<Hiss::IndexBuffer2>            index_buffer;

    void create_buffer(Engine& engine)
    {
        vertex_buffer = std::make_unique<Hiss::VertexBuffer2<Vertex3D>>(engine.device(), engine.allocator, vertices,
                                                                        "mesh vertices");
        index_buffer  = std::make_unique<Hiss::IndexBuffer2>(engine.device(), engine.allocator, faces, "mesh indcies");
    }
};


/**
 * 几何信息和材质信息的简单结合
 */
struct MatMesh
{
    std::unique_ptr<Mesh2> mesh;
    std::unique_ptr<Matt>  mat;
};


/**
 * 将 Assimp 的 vec3 转换为 glm 的 vec3
 */
inline glm::vec3 to_vec3(const aiVector3D& vec)
{
    return {vec.x, vec.y, vec.z};
}

inline glm::vec4 to_vec4(const aiColor4D& color)
{
    return {color.r, color.g, color.b, color.a};
}

/**
 * 将 Assimp 的矩阵转化为 glm 的矩阵
 * @details Assimp 的矩阵是 row-major 的，a 表示第 1 行，d 表示第 4 行
 * @details glm 的矩阵是 column-major 的
 */
inline glm::mat4 to_mat4(const aiMatrix4x4& mat)
{
    return glm::mat4{mat.a1, mat.b1, mat.c1, mat.d1,     // 第 1 列
                     mat.a2, mat.b2, mat.c2, mat.d2,     // 第 2 列
                     mat.a3, mat.b3, mat.c3, mat.d3,     // 第 3 列
                     mat.a4, mat.b4, mat.c4, mat.d4};    // 第 4 列
}


/**
 * 场景节点
 */
struct ModelNode
{
    glm::mat4            relative_matrix{};    // 相对于父节点的位姿
    std::vector<MatMesh> meshes;

    std::vector<std::unique_ptr<ModelNode>> children;

    void draw(const std::function<void(const MatMesh&, const glm::mat4&)>& d)
    {
        // TODO 暂时不考虑相对位置

        for (auto& mesh: meshes)
        {
            d(mesh, relative_matrix);
        }

        for (auto& child: children)
        {
            child->draw(d);
        }
    }
};


class MeshLoader
{
public:
    MeshLoader(Hiss::Engine& engine, const std::filesystem::path& mesh_path)
        : engine(engine),
          mesh_path(mesh_path),
          dir_path(mesh_path.parent_path())
    {
        if (!exists(mesh_path))
            throw std::runtime_error("mesh file not exist: " + mesh_path.string());


        _load();
    }


private:
    void _load()
    {
        // importer 析构时，会自动回收资源
        Assimp::Importer assimp_impoter;

        /**
         * Assimp 导入的后处理操作
         * @details 默认坐标系是右手系：+x = right, +y = up, +z 朝向观察方向，可以通过 MakeLeftHanded 标志来修改
         * @details 默认三角形环绕方向是 CCW，可以通过 FlipWindingOrder 标志来修改
         * @details 默认 UV 以左下角为原点，可以通过 FlipUVs 标志修改为左上角
         * @details 默认矩阵采用 row major 的存储方式
         * @flag calcTangentSpace 如果顶点具有法线属性，自动生成 tangent space 属性
         * @flag JoinIdenticalVertices 如果没有这个 flag，那么渲染时是不需要 index buffer 的
         * @flag Triangulate 将所有的面三角化
         * @flag GenNormals 如果没有法线，自动生成面法线
         * @flag SortByPType 在三角化之后发生，可以去除 point 和 line
         */
        auto post_process_flags = aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate
                                | aiProcess_GenNormals | aiProcess_SortByPType | aiProcess_FlipUVs;


        /**
         * 载入模型文件
         * @参考 Assimp 的文档：https://assimp-docs.readthedocs.io/en/v5.1.0/usage/use_the_lib.html
         */
        scene = assimp_impoter.ReadFile(mesh_path, post_process_flags);
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            spdlog::error("{}", assimp_impoter.GetErrorString());
            throw std::runtime_error("fail to _load obj file: " + mesh_path.string());
        }


        // 从根节点开始，递归地处理
        root_node = _process_node(*scene->mRootNode);
    }


    /**
     * 递归地处理节点，节点中包括多个 mesh，包含子节点
     */
    std::unique_ptr<ModelNode> _process_node(const aiNode& ai_node)
    {
        std::unique_ptr<ModelNode> node{new ModelNode()};
        node->relative_matrix = to_mat4(ai_node.mTransformation);

        // 处理 node 中的所有 mesh
        node->meshes.reserve(ai_node.mNumMeshes);
        for (int i = 0; i < ai_node.mNumMeshes; ++i)
        {
            auto ai_mesh = scene->mMeshes[ai_node.mMeshes[i]];
            node->meshes.push_back(_process_mesh(*ai_mesh));
        }

        // 处理所有的子节点
        node->children.reserve(ai_node.mNumChildren);
        for (int i = 0; i < ai_node.mNumChildren; ++i)
        {
            node->children.push_back(_process_node(*ai_node.mChildren[i]));
        }

        return node;
    }


    /**
     * 处理 aiMesh。一个 aiMesh 包括几何数据，以及对应的一个 Material 信息
     */
    MatMesh _process_mesh(const aiMesh& ai_mesh)
    {
        return MatMesh{
                .mesh = _get_geometry(ai_mesh),
                .mat  = _get_material(*scene->mMaterials[ai_mesh.mMaterialIndex]),
        };
    }


    /**
     * 从 aiMesh 中提取 material 的信息
     */
    std::unique_ptr<Matt> _get_material(const aiMaterial& ai_mat)
    {
        std::unique_ptr<Matt> mat{new Matt()};

        // 提取出各种颜色
        {
            aiColor4D out_color;

            // diffuse color
            ai_mat.Get(AI_MATKEY_COLOR_DIFFUSE, out_color);
            mat->color_diffuse = to_vec4(out_color);

            // ambient color
            ai_mat.Get(AI_MATKEY_COLOR_AMBIENT, out_color);
            mat->color_ambient = to_vec4(out_color);

            // specular color
            ai_mat.Get(AI_MATKEY_COLOR_SPECULAR, out_color);
            mat->color_specular = to_vec4(out_color);

            // emissive color
            ai_mat.Get(AI_MATKEY_COLOR_EMISSIVE, out_color);
            mat->color_emissive = to_vec4(out_color);
        }

        // 提取出各种纹理
        {
            // diffuse texture
            mat->tex_diffuse = _get_texture(ai_mat, aiTextureType_DIFFUSE);

            // emissive texture
            mat->tex_ambient = _get_texture(ai_mat, aiTextureType_AMBIENT);

            // specular texture
            mat->tex_specular = _get_texture(ai_mat, aiTextureType_SPECULAR);

            // emissive texture
            mat->tex_emissive = _get_texture(ai_mat, aiTextureType_EMISSIVE);
        }

        mat->create_descriptor_set(engine);
        return mat;
    }

    /**
     * 从 aiMaterial 中提取出特定类型的纹理
     */
    std::unique_ptr<Texture> _get_texture(const aiMaterial& ai_mat, aiTextureType tex_type,
                                          vk::Format format = vk::Format::eR8G8B8A8Srgb)
    {
        if (ai_mat.GetTextureCount(tex_type) == 0)
            return nullptr;

        aiString out_path;    // 获取到的是相对路径

        ai_mat.GetTexture(tex_type, 0, &out_path);
        return std::make_unique<Texture>(engine.device(), engine.allocator, dir_path / out_path.C_Str(), format);
    }


    /**
     * 从 aiMesh 中提取几何信息
     */
    std::unique_ptr<Mesh2> _get_geometry(const aiMesh& ai_mesh)
    {
        std::unique_ptr<Mesh2> mesh{new Mesh2()};

        auto vert_num = ai_mesh.mNumVertices;
        mesh->vertices.resize(vert_num);

        auto face_num = ai_mesh.mNumFaces;
        mesh->faces.resize(face_num);

        // faces
        for (int i = 0; i < face_num; ++i)
        {
            // 通过 Assimp 的 post-process 保证了这里的 face 都是 triangle
            assert(ai_mesh.mFaces[i].mNumIndices == 3);
            mesh->faces[i] = {
                    ai_mesh.mFaces[i].mIndices[0],
                    ai_mesh.mFaces[i].mIndices[1],
                    ai_mesh.mFaces[i].mIndices[2],
            };
        }

        // 通过 Assimp 的 post-process 保证了一定会有 normal，tangent
        assert(ai_mesh.HasNormals() && ai_mesh.HasTangentsAndBitangents());


        // vertices
        for (int i = 0; i < vert_num; ++i)
        {
            // position
            mesh->vertices[i].position = to_vec3(ai_mesh.mVertices[i]);

            // normal
            mesh->vertices[i].normal = to_vec3(ai_mesh.mNormals[i]);

            // tangent and biTangent
            mesh->vertices[i].tangent   = to_vec3(ai_mesh.mTangents[i]);
            mesh->vertices[i].bitangent = to_vec3(ai_mesh.mBitangents[i]);
        }


        // uv: Assimp 最多支持 8 套 uv。我们只需要第一套就好
        if (ai_mesh.HasTextureCoords(0))
        {
            for (int i = 0; i < vert_num; ++i)
            {
                mesh->vertices[i].uv = to_vec3(ai_mesh.mTextureCoords[0][i]);
            }
        }

        mesh->create_buffer(engine);
        return mesh;
    }


public:
    std::unique_ptr<ModelNode> root_node;


private:
    Hiss::Engine& engine;

    const std::filesystem::path mesh_path;    // mesh 文件对应的路径
    const std::filesystem::path dir_path;     // mesh 文件所在的文件夹，形式："xx/xxx"

    const aiScene* scene{};
};
}    // namespace Hiss
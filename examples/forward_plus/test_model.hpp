#pragma once
#include "core/vk_include.hpp"
#include <filesystem>
#include <fmt/format.h>
#include "mat.hpp"
#include "func/vertex.hpp"
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>


namespace Hiss
{
struct Cube
{
    const std::vector<glm::vec3> pos = {
            {-1, 1, 1},  {1, 1, 1},       //
            {1, -1, 1},  {-1, -1, 1},     //
            {-1, 1, -1}, {1, 1, -1},      //
            {1, -1, -1}, {-1, -1, -1},    //
    };

    const std::vector<glm::uvec3> faces = {
            {0, 3, 1}, {1, 3, 2},    // front
            {4, 5, 7}, {5, 6, 7},    // back
            {0, 4, 7}, {0, 7, 3},    // left
            {5, 1, 2}, {5, 2, 6},    // right
            {0, 1, 5}, {0, 5, 4},    // top
            {2, 3, 7}, {2, 7, 6},    // bottom
    };
};


class Mesh2
{
public:
    std::vector<Vertex3DNormalUV> vertices;
    std::vector<uint32_t>         indices;
};


class ModelNode
{
    glm::mat4              model;    // absolute
    ForwardPlus::Material* material;
    Mesh2*                  mesh;
    std::vector<Mesh2*>     children;
};


class MeshLoader
{
public:
    explicit MeshLoader(const std::filesystem::path& mesh_file)
        : mesh_file(mesh_file)
    {
        if (!exists(mesh_file))
            throw std::runtime_error("mesh file not exist: " + mesh_file.string());
    }


    void llll(const std::filesystem::path& obj_file)
    {
        Assimp::Importer assimp_impoter;

        /**
         * Assimp 导入的后处理操作
         * @注 默认是右手系：+x = right, +y = up, +z 朝向观察方向
         * @flag calcTangentSpace 如果顶点具有法线属性，自动生成 tangent space 属性
         * @flag JoinIdenticalVertices 如果没有这个 flag，那么渲染时是不需要 index buffer 的
         * @flag Triangulate 将所有的面三角化
         * @flag GenNormals 如果没有法线，自动生成面法线
         * @flag SortByPType 在三角化之后发生，可以去除 point 和 line
         */
        auto flags = aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate
                   | aiProcess_GenNormals | aiProcess_SortByPType;


        /**
         * 载入模型文件
         * @参考 Assimp 的文档：https://assimp-docs.readthedocs.io/en/v5.1.0/usage/use_the_lib.html
         */
        scene = assimp_impoter.ReadFile(obj_file, flags);
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            spdlog::error("{}", assimp_impoter.GetErrorString());
            throw std::runtime_error("fail to load obj file: " + obj_file.string());
        }
    }


    Mesh2* _process_mesh(const aiMesh& ai_mesh)
    {
        auto* mesh = new Mesh2();
        mesh->vertices.resize(ai_mesh.mNumVertices);


        /// 读取顶点数据
        for (int i = 0; i < ai_mesh.mNumVertices; ++i) {

        }
    }


private:
    const std::filesystem::path mesh_file;

    const aiScene* scene{};


    std::vector<Mesh2*>                  meshes;         // 场景中所有的 mesh
    std::vector<ForwardPlus::Material*> materials{};    // 场景中所有的 material
};
}    // namespace Hiss
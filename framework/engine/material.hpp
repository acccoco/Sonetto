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
#include "engine/descriptor_layout.hpp"
#include "utils/vk_func.hpp"
#include "material.hpp"

#define HISS_CPP
#include "shader/light_type.glsl"

namespace Hiss
{

/**
 * 材质信息
 * @注 应该将 CPU 和 GPU 的 material 区分开来
 */
struct Matt
{
    glm::vec4 color_ambient{0.f};
    glm::vec4 color_diffuse{0.f};
    glm::vec4 color_specular{0.f};
    glm::vec4 color_emissive{0.f};

    std::unique_ptr<Hiss::Texture> tex_diffuse;
    std::unique_ptr<Hiss::Texture> tex_ambient;
    std::unique_ptr<Hiss::Texture> tex_emissive;
    std::unique_ptr<Hiss::Texture> tex_specular;


    /**
     * 转化为 shader 中定义的 material 结构
     */
    Shader::Material to_shader_material()
    {
        return Shader::Material{
                .ambient_color        = color_ambient,
                .emissive_color       = color_emissive,
                .diffuse_color        = color_diffuse,
                .specular_color       = color_specular,
                .has_ambient_texture  = tex_emissive ? 1u : 0u,
                .has_emissive_texture = tex_emissive ? 1u : 0u,
                .has_diffuse_texture  = tex_diffuse ? 1u : 0u,
                .has_specular_texture = tex_specular ? 1u : 0u,
        };
    }

    vk::DescriptorSet                    descriptor_set;
    std::unique_ptr<Hiss::UniformBuffer> material_uniform;


    /**
     * 根据材质创建 descriptor set
     */
    void create_descriptor_set(Engine& engine)
    {
        auto shader_material = to_shader_material();

        // 根据材质创建 uniform buffer
        material_uniform =
                std::make_unique<Hiss::UniformBuffer>(engine.device(), engine.allocator, sizeof(Shader::Material),
                                                      "material uniform buffer", &shader_material);


        descriptor_set = engine.create_descriptor_set(engine.material_layout, "material descriptor set");


        std::vector<Hiss::Initial::DescriptorWrite> writes = {
                {.type = vk::DescriptorType::eUniformBuffer, .buffer = material_uniform.get()},
        };

        auto func = [&engine, &writes](std::unique_ptr<Texture>& tex, int binding) {
            writes.push_back(Hiss::Initial::DescriptorWrite{
                    .type    = vk::DescriptorType::eCombinedImageSampler,
                    .image   = &engine.default_texture->image(),
                    .sampler = engine.default_texture->sampler(),
                    .binding = binding,
            });
            if (tex)
            {
                writes.back().image   = &tex->image();
                writes.back().sampler = tex->sampler();
            }
        };

        func(tex_ambient, 1);
        func(tex_emissive, 2);
        func(tex_diffuse, 3);
        func(tex_specular, 4);


        Hiss::Initial::descriptor_set_write(engine.vkdevice(), descriptor_set, writes);
    }
};

}    // namespace Hiss

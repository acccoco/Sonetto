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
#include "utils/descriptor.hpp"
#include "material.hpp"

#define HISS_CPP
#include "shader/material_type.glsl"
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
                .specular_power       = 64.f,
                .has_ambient_texture  = tex_emissive ? 1u : 0u,
                .has_emissive_texture = tex_emissive ? 1u : 0u,
                .has_diffuse_texture  = tex_diffuse ? 1u : 0u,
                .has_specular_texture = tex_specular ? 1u : 0u,
        };
    }

    std::shared_ptr<DescriptorSet>       descriptor_set;
    std::unique_ptr<Hiss::UniformBuffer> material_uniform;


    /**
     * 使用 weak_ptr 不会占用引用计数，又可以分享 ptr，确保在 device 销毁之前被回收
     */
    inline static std::weak_ptr<Hiss::DescriptorLayout> material_descriptor;

    static std::shared_ptr<Hiss::DescriptorLayout> get_material_descriptor(Hiss::Device& device)
    {
        auto ptr = material_descriptor.lock();
        if (!ptr)
        {
            ptr = std::make_shared<Hiss::DescriptorLayout>(
                    device, std::vector<Hiss::Initial::BindingInfo>{
                                    {vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                                    {vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment},
                            });
            material_descriptor = ptr;
        }

        return ptr;
    }


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


        descriptor_set = std::make_shared<DescriptorSet>(engine, get_material_descriptor(engine.device()),
                                                         "material descriptor set");

        descriptor_set->write({.buffer = material_uniform.get()}, 0);

        // 如果有材质，就填充材质；否则填充默认材质
        auto func = [&engine, this](std::unique_ptr<Texture>& tex, int binding) {
            if (tex)
            {
                descriptor_set->write({.image = &tex->image(), .sampler = tex->sampler()}, binding);
            }
            else
            {
                descriptor_set->write(
                        {.image = &engine.default_texture->image(), .sampler = engine.default_texture->sampler()},
                        binding);
            }
        };

        func(tex_ambient, 1);
        func(tex_emissive, 2);
        func(tex_diffuse, 3);
        func(tex_specular, 4);
    }
};

}    // namespace Hiss

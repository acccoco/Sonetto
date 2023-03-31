#ifndef SHADER_MATERIAL_TYPE
#define SHADER_MATERIAL_TYPE
// 普通的材质


#ifndef HISS_CPP
#extension GL_GOOGLE_include_directive : enable
#endif    // HISS_CPP
#include "./_macro_.glsl"


NAMESPACE_BEGIN(Shader)

// 材质的定义
struct Material    // total size = 112
{
    ALIGN(16) vec4 global_ambient;    // 全局的环境光

    // offset = 16

    ALIGN(16) vec4 ambient_color;
    ALIGN(16) vec4 emissive_color;
    ALIGN(16) vec4 diffuse_color;
    ALIGN(16) vec4 specular_color;

    // offset = 80

    ALIGN(4) float opacity;           // 不透明度
    ALIGN(4) float specular_power;    // 就是 phong 模型中的，可以表示表面的光滑程度

    // offset = 88

    ALIGN(4) uint has_ambient_texture;
    ALIGN(4) uint has_emissive_texture;
    ALIGN(4) uint has_diffuse_texture;
    ALIGN(4) uint has_specular_texture;

    // offset = 104

    ALIGN(8) vec2 _padding_;
};

NAMESPACE_END    // Shader

#endif    // SHADER_MATERIAL_TYPE

#ifndef SHADER_LIGHT_TYPE
#define SHADER_LIGHT_TYPE


#ifndef HISS_CPP
#extension GL_GOOGLE_include_directive : enable
#endif    // HISS_CPP
#include "./_macro_.glsl"


NAMESPACE_BEGIN(Shader)

#define POINT_LIGHT 0
#define DIRECTIONAL_LIGHT 1
#define SPOT_LIGHT 2

// 光源的定义
struct Light    // total size = 96
{
    ALIGN(16) vec4 pos_world;
    ALIGN(16) vec4 dir_world;
    ALIGN(16) vec4 pos_view;
    ALIGN(16) vec4 dir_view;

    // offset = 64

    ALIGN(16) vec4 color;

    // offset = 80

    ALIGN(4) float spot_light_angle;    // spot light 的角度啊
    ALIGN(4) float range;               // 光源的范围
    ALIGN(4) float intensity;
    ALIGN(4) uint type;
};

// 光照计算的结果
struct LightingResult
{
    ALIGN(16) vec3 diffuse;
    ALIGN(16) vec3 specular;
};


NAMESPACE_END    // Shader


#endif    // SHADER_LIGHT_TYPE
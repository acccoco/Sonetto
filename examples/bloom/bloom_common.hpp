#pragma once
#include "engine/engine.hpp"
#define HISS_CPP
#include "shader/light_type.glsl"
#include "shader/common_type.glsl"


extern Hiss::Engine* g_engine;


namespace Bloom
{
const int WORKGROUP_SIZE = 8;


struct Resource
{

    /**
     * 自发光物体的材质，以及位置
     */
    std::vector<std::tuple<Shader::Material, glm::vec3>> emissive_blocks{
            {{.emissive_color = {1.0f, 1.0f, 1.0f, 1.f}}, {1.f, 1.f, 1.f}},    // white
            {{.emissive_color = {1.0f, 0.0f, 0.0f, 1.f}}, {2.f, 3.f, 4.f}},    // red
            {{.emissive_color = {0.0f, 1.0f, 0.0f, 1.f}}, {}},                 // green
            {{.emissive_color = {0.0f, 0.0f, 1.0f, 1.f}}, {}},                 // blue
    };
};
}    // namespace Bloom

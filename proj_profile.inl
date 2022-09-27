/**
 * 各种路径配置
 * 通过 cmake 的 configure 生成实际的路径
 */
#pragma once
#include <string>


const int32_t WINDOW_WIDTH  = 800;
const int32_t WINDOW_HEIGHT = 800;



inline std::string ASSETS(const std::string& obj)
{
    return "${PROJ_ASSETS_DIR}/" + obj;
}

inline std::string TEXTURE(const std::string& tex)
{
    return "${PROJ_ASSETS_DIR}/textures/" + tex;
}

inline std::string MODEL(const std::string& model)
{
    return "${PROJ_ASSETS_DIR}/model/" + model;
}

inline std::string SHADER(const std::string& shader)
{
    return "${PROJ_SHADER_DIR}/" + shader;
}

/**
 * 各种路径配置
 * 通过 cmake 的 configure 生成实际的路径
 */
#pragma once
#include <string>
#include <filesystem>

#include <spdlog/spdlog.h>


// 默认的窗口大小
const int32_t WINDOW_WIDTH  = 800;
const int32_t WINDOW_HEIGHT = 800;


// spdlog 的配置
const struct
{
    spdlog::level::level_enum level   = spdlog::level::trace;
    std::string               pattern = "%^%v%$";
} default_log_config;


// 项目的文件夹配置
const std::filesystem::path assets("${PROJ_ASSETS_DIR}");
const std::filesystem::path shader_dir("${PROJ_SHADER_DIR}");
const std::filesystem::path texture = assets / "textures";


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

cmake_minimum_required(VERSION 3.22)
project(Hiss)   # Hiss for 🥵


# 强制使用 makefile（这样设置后，使用 Ninja 会报错）
# cmake 目前只支持通过 makefile 来推断 glsl 中的依赖关系
set (CMAKE_GENERATOR "Unix Makefiles" CACHE INTERNAL "" FORCE)


# 导入编译数据库，用于项目分析
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)


# compile options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")


# folder and profiles
set(PROJ_SHADER_DIR ${CMAKE_SOURCE_DIR}/shader)
set(PROJ_ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
configure_file(${CMAKE_SOURCE_DIR}/proj_config.inl ${CMAKE_BINARY_DIR}/in/proj_config.hpp)
include_directories(${CMAKE_BINARY_DIR}/in)
set(PROJ_FRAMEWORK framework)


include(cmake/sampler_helper.cmake)
add_subdirectory(framework)
add_subdirectory(examples)


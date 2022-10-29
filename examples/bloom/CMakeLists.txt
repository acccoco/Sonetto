get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)


compile_shader(
        TARGET_NAME ${FOLDER_NAME}
        SHADER_DIR ${PROJ_SHADER_DIR}/bloom
        SHADER_NAMES
)

add_sample(
        TARGET_NAME ${FOLDER_NAME}
        SOURCES "bloom.cpp" "bloom_common.hpp"
)
get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)


compile_shader(
        TARGET_NAME ${FOLDER_NAME}
        SHADER_DIR ${PROJ_SHADER_DIR}/hello_material
        SHADER_NAMES "hello_material.vert" "hello_material.frag"
)

add_sample(
        TARGET_NAME ${FOLDER_NAME}
        SOURCES "app.cpp" "color_pass.hpp"
)
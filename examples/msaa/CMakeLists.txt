get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)


add_sample(
        TARGET_NAME ${FOLDER_NAME}
        SHADER_DIR ${PROJ_SHADER_DIR}/msaa
        SOURCES "msaa.cpp" "msaa.hpp"
        SHADER_NAMES msaa.vert msaa.frag
)


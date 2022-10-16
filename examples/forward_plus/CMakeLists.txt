get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)


compile_shader(
        TARGET_NAME ${FOLDER_NAME}
        SHADER_DIR ${PROJ_SHADER_DIR}/forward_plus
        SHADER_NAMES "frustum.comp" "light_cull.comp" "final.frag" "final.vert" "depth.vert" "depth.frag"
)

add_sample(
        TARGET_NAME ${FOLDER_NAME}
        SOURCES "forward_plus.cpp" "light.hpp" "mat.hpp" "camera.hpp" "test_model.hpp" "frustums_pass.hpp" "forward_common.hpp" "depth_pass.hpp" "cull_pass.hpp" "final_pass.hpp"
)
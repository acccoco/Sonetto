get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)


compile_shader(
    TARGET_NAME ${FOLDER_NAME}
    SHADER_DIR "${PROJ_SHADER_DIR}/compute_particle"
    SHADER_NAMES "particle.vert" "particle.frag" "calculate.comp" "integrate.comp" "surface.comp"
)

add_sample(
        TARGET_NAME ${FOLDER_NAME}
        SOURCES "surface_pass.hpp" "graphics_pass.hpp" "nbody_pass.hpp" "particle.hpp" "compute_particle.cpp"
)
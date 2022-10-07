get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)


add_sample(
        TARGET_NAME ${FOLDER_NAME}
        SHADER_DIR "${PROJ_SHADER_DIR}/compute_particle"
        SOURCES "surface.hpp" "graphics.hpp" "nbody.hpp" "particle.hpp" "app.cpp"
        SHADER_NAMES "particle.vert" "particle.frag" "calculate.comp" "integrate.comp" "surface.comp"
)
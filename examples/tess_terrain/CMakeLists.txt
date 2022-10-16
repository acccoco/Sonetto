get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

compile_shader(
        TARGET_NAME ${FOLDER_NAME}
        SHADER_DIR ${PROJ_SHADER_DIR}/tess_terrain
        SHADER_NAMES "terrain.vert" "terrain.frag" "terrain.tesc" "terrain.tese"
)

add_sample(
        TARGET_NAME ${FOLDER_NAME}
        SOURCES "terrain.cpp"
)




# compile shader
function(compile_shader)
    # 设置参数
    set(options)
    set(oneValueArgs TARGET_NAME SHADER_DIR)
    set(multiValueArgs SHADER_NAMES)

    # 解析参数
    cmake_parse_arguments(THIS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # 检查参数
    if (NOT THIS_TARGET_NAME
            OR NOT THIS_SHADER_DIR)
        message(FATAL_ERROR "params error")
    endif ()


    # 实际的代码
    set(SPV_FILES)
    set(SHADER_FILES)
    foreach (SHADER_NAME ${THIS_SHADER_NAMES})
        set(SPV_FILE ${THIS_SHADER_DIR}/${SHADER_NAME}.spv)
        set(SHADER_FILE ${THIS_SHADER_DIR}/${SHADER_NAME})
        list(APPEND SPV_FILES ${SPV_FILE})
        list(APPEND SHADER_FILES ${SHADER_FILE})

        add_custom_command(
                OUTPUT ${SPV_FILE}
                COMMAND glslc ${SHADER_FILE} -o ${SPV_FILE}
                # 这里默认监测 shader include 文件夹
                DEPENDS ${SHADER_FILE} ${PROJ_SHADER_DIR}/include
                VERBATIM
        )
    endforeach ()


    add_custom_target(${THIS_TARGET_NAME}
            DEPENDS ${SPV_FILES}
            SOURCES ${SHADER_FILES}     # 让 IDE 的 source file 界面有这些文件
            )
endfunction()


function(add_sample)
    set(options)
    set(oneValueArgs
            TARGET_NAME
            SHADER_DIR)
    set(multiValueArgs
            SOURCES
            SHADER_NAMES)
    cmake_parse_arguments(THIS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if (NOT THIS_TARGET_NAME
            OR NOT THIS_SHADER_DIR
            OR NOT THIS_SOURCES)
        message(FATAL_ERROR "params error")
    endif ()

    compile_shader(
            TARGET_NAME ${THIS_TARGET_NAME}.shader
            SHADER_DIR ${THIS_SHADER_DIR}
            SHADER_NAMES ${THIS_SHADER_NAMES}
    )
    add_executable(${THIS_TARGET_NAME} ${THIS_SOURCES})
    add_dependencies(${THIS_TARGET_NAME} ${THIS_TARGET_NAME}.shader)
    target_link_libraries(${THIS_TARGET_NAME} ${PROJ_FRAMEWORK})
endfunction()
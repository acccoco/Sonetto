# 关于参数解析
# cmake 的函数会将参数放在 ARGN 变量中，可以使用 cmake_parse_arguments 函数来提取参数


# compile shader
# target 名称为 ${TARGET_NAME}.shader
function(compile_shader)
    set(options)
    set(oneValueArgs
            TARGET_NAME
            SHADER_DIR  # 着色器所在的文件夹
            )
    set(multiValueArgs
            SHADER_NAMES)
    cmake_parse_arguments(THIS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})


    set(SPV_FILES)
    set(SHADER_FILES)
    foreach (SHADER_NAME ${THIS_SHADER_NAMES})
        set(SPV_FILE ${THIS_SHADER_DIR}/${SHADER_NAME}.spv)
        set(SHADER_FILE ${THIS_SHADER_DIR}/${SHADER_NAME})
        list(APPEND SPV_FILES ${SPV_FILE})
        list(APPEND SHADER_FILES ${SHADER_FILE})

        add_custom_command(
                OUTPUT ${SPV_FILE}
                COMMAND glslc -g --target-env=vulkan1.1 --target-spv=spv1.3 ${SHADER_FILE} -o ${SPV_FILE}
                DEPENDS ${SHADER_FILE}
                # 根据文件中的 #include 来推断依赖关系，目前只支持 makefile
                IMPLICIT_DEPENDS CXX ${SHADER_FILE}
                VERBATIM
        )
    endforeach ()


    add_custom_target(${THIS_TARGET_NAME}.shader
            DEPENDS ${SPV_FILES}
            SOURCES ${SHADER_FILES}     # 让 IDE 的 source file 界面有这些文件
            )
endfunction()


# 为 example 生成 target
function(add_sample)
    set(options)
    set(oneValueArgs
            TARGET_NAME
            )
    set(multiValueArgs
            SOURCES
            )
    cmake_parse_arguments(THIS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})


    add_executable(${THIS_TARGET_NAME} ${THIS_SOURCES})
    add_dependencies(${THIS_TARGET_NAME} ${THIS_TARGET_NAME}.shader)
    target_link_libraries(${THIS_TARGET_NAME} ${PROJ_FRAMEWORK})
    # 可以直接引用 shader 文件
    target_include_directories(${THIS_TARGET_NAME} PRIVATE ${PROJ_SHADER_DIR})
endfunction()
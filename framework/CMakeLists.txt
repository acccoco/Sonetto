
# system header and lib
if (APPLE)
    add_compile_definitions(VK_ICD_FILENAMES=/usr/local/share/vulkan/icd.d/MoltenVK_icd.json)
    add_compile_definitions(VK_LAYER_PATH=/usr/local/share/vulkan/explicit_layer.d)
    # enable best practice validation layer
    add_compile_definitions(VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT:VALIDATION_CHECK_ENABLE_VENDOR_SPECIFIC_ARM)
endif ()


# third party
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)
find_package(fmt REQUIRED)
find_package(Vulkan REQUIRED)
find_package(spdlog REQUIRED)
find_package(assimp REQUIRED)
find_package(TinyGLTF REQUIRED HINTS ${CMAKE_SOURCE_DIR}/third_party/tinyGLTF)
find_package(TinyObjLoader REQUIRED HINTS ${CMAKE_SOURCE_DIR}/third_party/tinyobj)
find_package(vma REQUIRED HINTS ${CMAKE_SOURCE_DIR}/third_party/vma)
set(LIBS
        glm::glm
        glfw
        fmt::fmt
        Vulkan::Vulkan
        spdlog::spdlog
        ${ASSIMP_LIBRARIES}
        TinyGLTF
        TinyObjLoader
        vma
        )


set(HEADER_FILES
        vk_config.hpp
        run.hpp
        utils/application.hpp

        utils/tools.hpp
        utils/timer.hpp
        utils/rand.hpp
        utils/shader_loader.hpp
        utils/semaphore_pool.hpp
        utils/stbi.hpp

        engine/image.hpp
        engine/swapchain.hpp
        core/vk_common.hpp
        core/instance.hpp
        core/window.hpp
        core/device.hpp
        core/gpu.hpp
        engine/engine.hpp
        core/command.hpp
        engine/frame.hpp
        core/queue.hpp
        core/vk_include.hpp

        engine/buffer.hpp
        engine/model.hpp
        engine/texture.hpp
        engine/vertex.hpp
        utils/pipeline_template.hpp
        engine/vertex_buffer.hpp
        utils/template.hpp
        utils/vk_func.hpp engine/frame_manager.hpp utils/light_pass.hpp engine/material.hpp utils/post_process.hpp utils/descriptor.hpp core/vkcore.hpp)

# source files
set(SOURCE_FILES
        utils/fence_pool.cpp

        engine/image.cpp
        core/instance.cpp
        core/window.cpp
        core/device.cpp
        core/gpu.cpp
        engine/swapchain.cpp
        engine/engine.cpp
        core/command.cpp

        engine/vertex.cpp
        engine/texture.cpp
        utils/pipeline_template.cpp
        engine/model.cpp
        run.cpp core/vkcore.cpp)


# static library
add_library(${PROJ_FRAMEWORK} STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${PROJ_FRAMEWORK} PUBLIC ${LIBS})
target_include_directories(${PROJ_FRAMEWORK} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/lib)

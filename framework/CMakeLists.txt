
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
find_package(Vulkan REQUIRED)
find_package(spdlog REQUIRED)
find_package(TinyGLTF REQUIRED HINTS ${CMAKE_SOURCE_DIR}/third_party/tinyGLTF)
find_package(TinyObjLoader REQUIRED HINTS ${CMAKE_SOURCE_DIR}/third_party/tinyobj)
set(LIBS
        glm::glm
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
        TinyGLTF
        TinyObjLoader
        )

set(LIB_HEADERS
        lib/vma/vk_mem_alloc.h
        )

set(LIB_SOURCES
        lib/vma/vk_mem_alloc.cpp
        )

set(HEADER_FILES
        vk/buffer.hpp
        vk/framebuffer.hpp
        vk/image.hpp
        vk/model.hpp
        vk/swapchain.hpp
        vk/texture.hpp
        utils/tools.hpp
        vk/vertex.hpp
        vk/vk_common.hpp
        vk/instance.hpp
        window.hpp
        vk/device.hpp
        vk/gpu.hpp
        application.hpp
        vk/pipeline.hpp
        vk/command.hpp utils/timer.hpp utils/rand.hpp utils/shader_loader.hpp frame.hpp utils/semaphore_pool.hpp vk/vk_config.hpp run.hpp vk/queue.hpp)

# source files
set(SOURCE_FILES
        vk/buffer.cpp
        vk/image.cpp
        vk/framebuffer.cpp
        vk/vertex.cpp
        vk/texture.cpp
        vk/instance.cpp
        window.cpp
        vk/device.cpp
        vk/vk_common.cpp
        vk/gpu.cpp
        vk/swapchain.cpp
        application.cpp
        vk/pipeline.cpp
        vk/command.cpp
        vk/fence_pool.cpp utils/tools.cpp vk/model.cpp)


# static library
add_library(${PROJ_FRAMEWORK} STATIC ${SOURCE_FILES} ${HEADER_FILES} ${LIB_HEADERS} ${LIB_SOURCES})
target_link_libraries(${PROJ_FRAMEWORK} PUBLIC ${LIBS})
target_include_directories(${PROJ_FRAMEWORK} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# -*- cmake -*-


FIND_PACKAGE(PackageHandleStandardArgs)


FIND_FILE(vma_source vk_mem_alloc.cpp PATHS ${vma_DIR})
FIND_FILE(vma_header vk_mem_alloc.h PATHS ${vma_DIR})

IF (NOT vma_source OR NOT vma_header)
    MESSAGE(FATAL_ERROR "Unable to find vma")
ENDIF ()
message(STATUS "find vma")

find_package(Vulkan REQUIRED)

add_library(vma STATIC ${vma_DIR}/vk_mem_alloc.cpp)
target_link_libraries(vma Vulkan::Vulkan)
target_include_directories(vma PUBLIC ${vma_DIR})

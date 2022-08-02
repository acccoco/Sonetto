# -*- cmake -*-
# - Find VulkanMemoryAllocator
# vmaConfig.cmake 所在的文件夹就是 vma_DIR


FIND_PACKAGE(PackageHandleStandardArgs)


FIND_FILE(source vk_mem_alloc.cpp PATHS ${vma_DIR})
FIND_FILE(header vk_mem_alloc.h PATHS ${vma_DIR})

IF (NOT source OR NOT header)
    MESSAGE(FATAL_ERROR "Unable to find vma files")
ENDIF ()
message(STATUS "find vma")

add_library(vma STATIC ${source})
target_include_directories(vma PUBLIC ${vma_DIR})

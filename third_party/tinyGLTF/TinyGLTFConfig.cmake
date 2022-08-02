# -*- cmake -*-
# - Find TinyGLTF

# TinyGLTF_INCLUDE_DIR      TinyGLTF's include directory


find_package(PackageHandleStandardArgs)

set(TinyGLTF_INC_DIR ${TinyGLTF_DIR}/include)
set(TinyGLTF_SRC_DIR ${TinyGLTF_DIR}/src)

find_file(TinyGLTF_HEADER tiny_gltf.h PATHS ${TinyGLTF_INC_DIR})

IF (NOT TinyGLTF_HEADER)
    MESSAGE(FATAL_ERROR " Unable to find tiny_gltf.h, TinyGLTF_INCLUDE_DIR = ${TinyGLTF_INC_DIR} ")
ENDIF ()
message(STATUS "find TinyGLTF")

add_library(TinyGLTF STATIC ${TinyGLTF_SRC_DIR}/tiny_gltf.cpp)
target_include_directories(TinyGLTF PUBLIC ${TinyGLTF_INC_DIR})

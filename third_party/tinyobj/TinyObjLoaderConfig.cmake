# -*- cmake -*-
# - Find TinyObjLoader
# TinyObjLoaderConfig.cmake 所在的文件夹就是 TinyObjLoader_DIR


FIND_PACKAGE(PackageHandleStandardArgs)


FIND_FILE(tiny_obj_source tiny_obj_loader.cpp PATHS ${TinyObjLoader_DIR})
FIND_FILE(tiny_obj_header tiny_obj_loader.h PATHS ${TinyObjLoader_DIR})

IF (NOT tiny_obj_source OR NOT tiny_obj_header)
    MESSAGE(FATAL_ERROR "Unable to find tiny_obj_loader")
ENDIF ()
message(STATUS "find TinyObjLoader")

add_library(TinyObjLoader STATIC ${TinyObjLoader_DIR}/tiny_obj_loader.cpp)
target_include_directories(TinyGLTF PUBLIC ${TinyObjLoader_DIR})

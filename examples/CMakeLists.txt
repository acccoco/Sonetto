
file(GLOB example_path_list ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach (example_path ${example_path_list})
    if (NOT IS_DIRECTORY ${example_path})
        continue()
    endif ()
    get_filename_component(example_name ${example_path} NAME)
    add_subdirectory(${example_name})
endforeach ()


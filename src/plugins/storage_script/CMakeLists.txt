cmake_minimum_required(VERSION 3.14)

add_library(storage_script SHARED)
target_sources(storage_script PUBLIC storage_script.cpp)

target_link_libraries(storage_script libswe)
set_target_properties(storage_script PROPERTIES PREFIX "")
set_target_properties(storage_script PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

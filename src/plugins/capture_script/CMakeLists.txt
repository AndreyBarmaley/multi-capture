cmake_minimum_required(VERSION 3.14)

add_library(capture_script SHARED)
target_sources(capture_script PUBLIC capture_script.cpp)

add_dependencies(capture_script libswe)
target_link_options(capture_script PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(capture_script libswe.so)

set_target_properties(capture_script PROPERTIES PREFIX "")
set_target_properties(capture_script PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

cmake_minimum_required(VERSION 3.14)

add_library(capture_flycap SHARED)
target_sources(capture_flycap PUBLIC capture_flycap.cpp)

target_include_directories(capture_flycap PUBLIC ./dev/include/flycapture ./dev/include/flycapture/C)
target_link_directories(capture_flycap PUBLIC ./dev/lib64)
target_link_libraries(capture_flycap flycapture flycapture-c)

add_dependencies(capture_flycap libswe)
target_link_options(capture_flycap PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(capture_flycap libswe.so)

set_target_properties(capture_flycap PROPERTIES PREFIX "")
set_target_properties(capture_flycap PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

cmake_minimum_required(VERSION 3.14)

add_library(capture_flycap SHARED)
target_sources(capture_flycap PUBLIC capture_flycap.cpp)

target_link_libraries(capture_flycap libswe)
set_target_properties(capture_flycap PROPERTIES PREFIX "")
set_target_properties(capture_flycap PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

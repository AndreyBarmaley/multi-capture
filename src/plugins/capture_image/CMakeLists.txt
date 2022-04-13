cmake_minimum_required(VERSION 3.14)

add_library(capture_image SHARED)
target_sources(capture_image PUBLIC capture_image.cpp)

target_link_libraries(capture_image libswe)
set_target_properties(capture_image PROPERTIES PREFIX "")
set_target_properties(capture_image PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

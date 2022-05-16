cmake_minimum_required(VERSION 3.14)

add_library(capture_image SHARED)
target_sources(capture_image PUBLIC capture_image.cpp)

add_dependencies(capture_image libswe)
target_link_options(capture_image PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(capture_image libswe.so)

set_target_properties(capture_image PROPERTIES PREFIX "")
set_target_properties(capture_image PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

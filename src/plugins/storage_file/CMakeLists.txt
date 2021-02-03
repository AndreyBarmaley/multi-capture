cmake_minimum_required(VERSION 3.14)

add_library(storage_file SHARED)
target_sources(storage_file PUBLIC storage_file.cpp)

target_link_libraries(storage_file libswe)
set_target_properties(storage_file PROPERTIES PREFIX "")
set_target_properties(storage_file PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

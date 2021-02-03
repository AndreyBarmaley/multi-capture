cmake_minimum_required(VERSION 3.14)

add_library(signal_input_event SHARED)
target_sources(signal_input_event PUBLIC signal_input_event.cpp)

target_link_libraries(signal_input_event libswe)
set_target_properties(signal_input_event PROPERTIES PREFIX "")
set_target_properties(signal_input_event PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

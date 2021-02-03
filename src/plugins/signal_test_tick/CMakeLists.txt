cmake_minimum_required(VERSION 3.14)

add_library(signal_test_tick SHARED)
target_sources(signal_test_tick PUBLIC signal_test_tick.cpp)

target_link_libraries(signal_test_tick libswe)
set_target_properties(signal_test_tick PROPERTIES PREFIX "")
set_target_properties(signal_test_tick PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

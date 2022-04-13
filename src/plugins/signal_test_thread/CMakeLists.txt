cmake_minimum_required(VERSION 3.14)

add_library(signal_test_thread SHARED)
target_sources(signal_test_thread PUBLIC signal_test_thread.cpp)

target_link_libraries(signal_test_thread libswe)
set_target_properties(signal_test_thread PROPERTIES PREFIX "")
set_target_properties(signal_test_thread PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

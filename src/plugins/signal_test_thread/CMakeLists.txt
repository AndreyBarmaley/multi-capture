cmake_minimum_required(VERSION 3.14)

add_library(signal_test_thread SHARED)
target_sources(signal_test_thread PUBLIC signal_test_thread.cpp)

add_dependencies(signal_test_thread libswe)
target_link_options(signal_test_thread PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(signal_test_thread libswe.so)

set_target_properties(signal_test_thread PROPERTIES PREFIX "")
set_target_properties(signal_test_thread PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

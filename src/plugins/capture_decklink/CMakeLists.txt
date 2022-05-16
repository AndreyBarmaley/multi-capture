cmake_minimum_required(VERSION 3.14)

add_library(capture_decklink SHARED)
target_sources(capture_decklink PUBLIC capture_decklink.cpp)
target_include_directories(capture_decklink PUBLIC include/decklink)

add_dependencies(capture_decklink libswe)
target_link_options(capture_decklink PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(capture_decklink libswe.so)

set_target_properties(capture_decklink PROPERTIES PREFIX "")
set_target_properties(capture_decklink PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

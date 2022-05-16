cmake_minimum_required(VERSION 3.14)

add_library(signal_gui_button SHARED)
target_sources(signal_gui_button PUBLIC signal_gui_button.cpp)

add_dependencies(signal_gui_button libswe)
target_link_options(signal_gui_button PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(signal_gui_button libswe.so)

set_target_properties(signal_gui_button PROPERTIES PREFIX "")
set_target_properties(signal_gui_button PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

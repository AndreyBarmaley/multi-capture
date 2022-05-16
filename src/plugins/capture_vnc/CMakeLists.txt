cmake_minimum_required(VERSION 3.14)

add_library(capture_vnc SHARED)
target_sources(capture_vnc PUBLIC capture_vnc.cpp capture_vnc_connector.cpp ./lib/libvnc.cpp ./lib/network_stream.cpp)

target_include_directories(capture_vnc PUBLIC ./lib)
target_link_libraries(capture_vnc gnutls)
target_link_libraries(capture_vnc SDL2_net)

add_dependencies(capture_vnc libswe)
target_link_options(capture_vnc PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(capture_vnc libswe.so)

set_target_properties(capture_vnc PROPERTIES PREFIX "")
set_target_properties(capture_vnc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

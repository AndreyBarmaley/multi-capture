cmake_minimum_required(VERSION 3.14)

add_library(storage_vnc SHARED)
target_sources(storage_vnc PUBLIC storage_vnc.cpp storage_vnc_connector.cpp storage_vnc_encodings.cpp ./lib/libvnc.cpp ./lib/network_stream.cpp)
target_include_directories(storage_vnc PUBLIC ./lib)

target_link_libraries(storage_vnc libswe)
target_link_libraries(storage_vnc gnutls)
target_link_libraries(storage_vnc SDL2_net)
set_target_properties(storage_vnc PROPERTIES PREFIX "")
set_target_properties(storage_vnc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

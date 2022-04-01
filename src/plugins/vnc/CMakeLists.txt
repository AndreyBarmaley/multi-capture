cmake_minimum_required(VERSION 3.14)

add_library(capture_vnc SHARED)
target_sources(capture_vnc PUBLIC capture/capture_vnc.cpp capture/capture_vnc_connector.cpp lib/libvnc.cpp lib/network_stream.cpp)
target_include_directories(capture_vnc PUBLIC lib)

target_link_libraries(capture_vnc libswe)
set_target_properties(capture_vnc PROPERTIES PREFIX "")
set_target_properties(capture_vnc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

add_library(storage_vnc SHARED)
target_sources(storage_vnc PUBLIC storage/storage_vnc.cpp storage/storage_vnc_connector.cpp storage/storage_vnc_encodings.cpp lib/libvnc.cpp lib/network_stream.cpp)
target_include_directories(storage_vnc PUBLIC lib)

target_link_libraries(storage_vnc libswe)
set_target_properties(storage_vnc PROPERTIES PREFIX "")
set_target_properties(storage_vnc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

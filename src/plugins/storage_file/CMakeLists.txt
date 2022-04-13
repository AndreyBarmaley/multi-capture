cmake_minimum_required(VERSION 3.14)

add_library(storage_file SHARED)
target_sources(storage_file PUBLIC storage_file.cpp storage_deinterlace.cpp)

pkg_search_module(FFMPEG REQUIRED libswscale libavutil)

target_link_libraries(storage_file libswe)
target_compile_options(storage_file PUBLIC ${FFMPEG_CFLAGS})
target_link_options(storage_file PUBLIC ${FFMPEG_LDFLAGS})
target_link_libraries(storage_file ${FFMPEG_LIBRARIES})

set_target_properties(storage_file PROPERTIES PREFIX "")
set_target_properties(storage_file PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

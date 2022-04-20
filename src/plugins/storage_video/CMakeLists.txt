cmake_minimum_required(VERSION 3.14)

add_library(storage_video SHARED)
target_sources(storage_video PUBLIC storage_video.cpp storage_format.cpp)

pkg_search_module(FFMPEG REQUIRED libavdevice libavformat libavcodec libswscale)
#target_compile_options(storage_video PUBLIC "-D__STDC_CONSTANT_MACROS")
target_compile_options(storage_video PUBLIC ${FFMPEG_CFLAGS})
target_link_options(storage_video PUBLIC ${FFMPEG_LDFLAGS})
target_link_libraries(storage_video ${FFMPEG_LIBRARIES})
target_link_libraries(storage_video libswe)

set_target_properties(storage_video PROPERTIES PREFIX "")
set_target_properties(storage_video PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

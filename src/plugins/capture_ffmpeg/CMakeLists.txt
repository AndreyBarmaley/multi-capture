cmake_minimum_required(VERSION 3.14)

add_library(capture_ffmpeg SHARED)
target_sources(capture_ffmpeg PUBLIC capture_ffmpeg.cpp)

pkg_search_module(FFMPEG REQUIRED libavdevice libavformat libavcodec libswscale libavutil)
#target_compile_options(capture_ffmpeg PUBLIC "-D__STDC_CONSTANT_MACROS")
target_compile_options(capture_ffmpeg PUBLIC ${FFMPEG_CFLAGS})
target_link_options(capture_ffmpeg PUBLIC ${FFMPEG_LDFLAGS})
target_link_libraries(capture_ffmpeg ${FFMPEG_LIBRARIES})
target_link_libraries(capture_ffmpeg libswe)

set_target_properties(capture_ffmpeg PROPERTIES PREFIX "")
set_target_properties(capture_ffmpeg PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

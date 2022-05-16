cmake_minimum_required(VERSION 3.14)

add_library(capture_ffmpeg SHARED)
target_sources(capture_ffmpeg PUBLIC capture_ffmpeg.cpp)

#pkg_search_module(FFMPEG REQUIRED libavdevice libavformat libavcodec libswscale libavutil)
pkg_search_module(AVDEVICE REQUIRED libavdevice)
pkg_search_module(AVFORMAT REQUIRED libavformat)
pkg_search_module(AVCODEC REQUIRED libavcodec)
pkg_search_module(AVSWSCALE REQUIRED libswscale)
pkg_search_module(AVUTIL REQUIRED libavutil)
#target_compile_options(capture_ffmpeg PUBLIC ${FFMPEG_CFLAGS})
#target_link_options(capture_ffmpeg PUBLIC ${FFMPEG_LDFLAGS})
#target_link_libraries(capture_ffmpeg ${FFMPEG_LIBRARIES})
#target_compile_options(capture_ffmpeg PUBLIC "-D__STDC_CONSTANT_MACROS")

target_compile_options(capture_ffmpeg PUBLIC ${AVDEVICE_CFLAGS} ${AVFORMAT_CFLAGS} ${AVCODEC_CFLAGS} ${AVSWSCALE_CFLAGS} ${AVUTIL_CFLAGS})
target_link_options(capture_ffmpeg PUBLIC ${AVDEVICE_LDFLAGS} ${AVFORMAT_LDFLAGS} ${AVCODEC_LDFLAGS} ${AVSWSCALE_LDFLAGS} ${AVUTIL_LDFLAGS})
target_link_libraries(capture_ffmpeg ${AVDEVICE_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES} ${AVSWSCALE_LIBRARIES} ${AVUTIL_LIBRARIES})

add_dependencies(capture_ffmpeg libswe)
target_link_options(capture_ffmpeg PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(capture_ffmpeg libswe.so)

set_target_properties(capture_ffmpeg PROPERTIES PREFIX "")
set_target_properties(capture_ffmpeg PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)



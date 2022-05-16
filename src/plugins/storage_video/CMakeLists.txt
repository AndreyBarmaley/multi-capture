cmake_minimum_required(VERSION 3.14)

add_library(storage_video SHARED)
target_sources(storage_video PUBLIC storage_video.cpp storage_format.cpp)

#pkg_search_module(FFMPEG REQUIRED libavdevice libavformat libavcodec libswscale)
#target_compile_options(storage_video PUBLIC "-D__STDC_CONSTANT_MACROS")
#target_compile_options(storage_video PUBLIC ${FFMPEG_CFLAGS})
#target_link_options(storage_video PUBLIC ${FFMPEG_LDFLAGS})
#target_link_libraries(storage_video ${FFMPEG_LIBRARIES})

pkg_search_module(AVDEVICE REQUIRED libavdevice)
pkg_search_module(AVFORMAT REQUIRED libavformat)
pkg_search_module(AVCODEC REQUIRED libavcodec)
pkg_search_module(AVSWSCALE REQUIRED libswscale)

target_compile_options(storage_video PUBLIC ${AVDEVICE_CFLAGS} ${AVFORMAT_CFLAGS} ${AVCODEC_CFLAGS} ${AVSWSCALE_CFLAGS})
target_link_options(storage_video PUBLIC ${AVDEVICE_LDFLAGS} ${AVFORMAT_LDFLAGS} ${AVCODEC_LDFLAGS} ${AVSWSCALE_LDFLAGS})
target_link_libraries(storage_video ${AVDEVICE_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES} ${AVSWSCALE_LIBRARIES})

add_dependencies(storage_video libswe)
target_link_options(storage_video PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins")
target_link_libraries(storage_video libswe.so)

set_target_properties(storage_video PROPERTIES PREFIX "")
set_target_properties(storage_video PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

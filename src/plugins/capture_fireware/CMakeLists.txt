cmake_minimum_required(VERSION 3.14)

add_library(capture_fireware SHARED)
target_sources(capture_fireware PUBLIC capture_fireware.cpp)

pkg_search_module(FIREWARE REQUIRED libraw1394 libavc1394 libiec61883 libdv libmpeg2 libmpeg2convert)
target_compile_options(capture_fireware PUBLIC ${FIREWARE_CFLAGS})
target_link_options(capture_fireware PUBLIC ${FIREWARE_LDFLAGS})
target_link_libraries(capture_fireware ${FIREWARE_LIBRARIES})
target_link_libraries(capture_fireware libswe)

set_target_properties(capture_fireware PROPERTIES PREFIX "")
set_target_properties(capture_fireware PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

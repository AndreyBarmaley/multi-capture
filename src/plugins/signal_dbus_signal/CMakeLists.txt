cmake_minimum_required(VERSION 3.14)

add_library(signal_dbus_signal SHARED)
target_sources(signal_dbus_signal PUBLIC signal_dbus_signal.cpp)

pkg_search_module(DBUS REQUIRED dbus-1)
target_compile_options(signal_dbus_signal PUBLIC ${DBUS_CFLAGS})
target_link_options(signal_dbus_signal PUBLIC ${DBUS_LDFLAGS})
target_link_libraries(signal_dbus_signal ${DBUS_LIBRARIES})
target_link_libraries(signal_dbus_signal libswe)

set_target_properties(signal_dbus_signal PROPERTIES PREFIX "")
set_target_properties(signal_dbus_signal PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

cmake_minimum_required(VERSION 3.14)

project(FlyCapturePlugins VERSION 20210201.1)

add_subdirectory(capture_ffmpeg)
add_subdirectory(capture_image)
add_subdirectory(capture_script)
add_subdirectory(storage_file)
add_subdirectory(storage_script)
add_subdirectory(signal_test_thread)
add_subdirectory(signal_test_tick)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_subdirectory(capture_fireware)
    add_subdirectory(signal_dbus_signal)
    add_subdirectory(signal_input_event)
endif()

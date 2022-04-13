cmake_minimum_required(VERSION 3.14)

project(MultiCapturePlugins VERSION 20210201.1)

add_subdirectory(capture_ffmpeg)
add_subdirectory(capture_image)
add_subdirectory(capture_script)
add_subdirectory(capture_vnc)
add_subdirectory(storage_file)
add_subdirectory(storage_video)
add_subdirectory(storage_script)
add_subdirectory(storage_vnc)
add_subdirectory(signal_test_thread)
add_subdirectory(signal_gui_button)

if(WITH_FLYCAP_PLUGIN)
    add_subdirectory(capture_flycap)
endif()

if(WITH_DECKLINK_PLUGIN)
    add_subdirectory(capture_decklink)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    add_subdirectory(capture_fireware)
    add_subdirectory(signal_dbus_signal)
    add_subdirectory(signal_dbus_control)
    add_subdirectory(signal_input_event)
endif()

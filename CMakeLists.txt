cmake_minimum_required(VERSION 3.14)

project(MultiCapture VERSION 20210201.1)

option(WITH_FLYCAP_PLUGIN "enable capture plugin for FlyCapture SDK" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SWE_DLOPEN ON CACHE BOOL "enable load shared libs" FORCE)
set(SWE_WITH_JSON ON CACHE BOOL "enable json" FORCE)
set(SWE_WITH_LUA OFF CACHE BOOL "disable lua" FORCE)
set(SWE_EXAMPLES OFF CACHE BOOL "disable examples" FORCE)
set(SWE_DISABLE_TERMGUI OFF CACHE BOOL "enable termwin_gui" FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DBUILD_DEBUG")

add_subdirectory(engine)

include(engine/libswe.cmake)

set(MULTICAP_SOURCE
    src/multi_capture.cpp
    src/gallerywindow.cpp
    src/mainscreen.cpp
    src/plugins.cpp
    src/settings.cpp
    src/videowindow.cpp)

include_directories(engine src)
add_executable(MultiCapture ${MULTICAP_SOURCE})

target_link_libraries(MultiCapture libswe)
set_target_properties(MultiCapture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src/plugins)

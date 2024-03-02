cmake_minimum_required(VERSION 3.14)

project(libswe VERSION 20220620.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SWE_DLOPEN ON CACHE BOOL "enable load shared libs" FORCE)
set(SWE_WITH_JSON ON CACHE BOOL "enable json" FORCE)
set(SWE_WITH_LUA OFF CACHE BOOL "disable lua" FORCE)
set(SWE_EXAMPLES OFF CACHE BOOL "disable examples" FORCE)
set(SWE_DISABLE_TERMGUI OFF CACHE BOOL "enable termwin_gui" FORCE)
set(SWE_SHARED ON CACHE BOOL "enable shared" FORCE)
set(SWE_BUILTIN_ROOTOZOOM2 OFF CACHE BOOL "built in rotozoom" FORCE)

add_subdirectory(engine)
include(engine/libswe.cmake)
set_target_properties(libswe PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist/plugins)

project(MultiCapture VERSION 20210201.1)

option(WITH_FLYCAP_PLUGIN "enable capture plugin for FlyCapture SDK" OFF)
option(WITH_DECKLINK_PLUGIN "enable capture plugin for DeckLink SDK" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DBUILD_DEBUG")

set(MULTICAP_SOURCE
    src/multi_capture.cpp
    src/gallerywindow.cpp
    src/mainscreen.cpp
    src/plugins.cpp
    src/settings.cpp
    src/videowindow.cpp)

find_package(Threads REQUIRED)

include_directories(engine src)
add_executable(MultiCapture ${MULTICAP_SOURCE})
target_link_libraries(MultiCapture Threads::Threads)

# libswe
add_dependencies(MultiCapture libswe)
target_link_options(MultiCapture PUBLIC "-L${CMAKE_CURRENT_SOURCE_DIR}/dist/plugins")
target_link_libraries(MultiCapture libswe.so)

set_target_properties(MultiCapture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist)

add_subdirectory(src/plugins)

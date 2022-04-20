cmake_minimum_required(VERSION 3.14)

add_library(storage_file SHARED)
target_sources(storage_file PUBLIC storage_file.cpp)

pkg_search_module(GFX REQUIRED SDL_gfx)

target_link_libraries(storage_file libswe)
target_compile_options(storage_file PUBLIC ${GFX_CFLAGS})
target_link_options(storage_file PUBLIC ${GFX_LDFLAGS})
target_link_libraries(storage_file ${GFX_LIBRARIES})

set_target_properties(storage_file PROPERTIES PREFIX "")
set_target_properties(storage_file PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../dist/plugins)

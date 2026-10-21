include(Dependencies.cmake)
add_library(centui_bgfx STATIC)
add_library(centui::bgfx ALIAS centui_bgfx)
centui_sources(centui_bgfx "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(centui_bgfx PUBLIC centui::centui PRIVATE bgfx centui_config)

include(Shaders.cmake)
centui_compile_shaders(centui_bgfx)

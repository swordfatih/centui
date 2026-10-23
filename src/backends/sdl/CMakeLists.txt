add_library(centui_sdl_renderer STATIC)
add_library(centui::sdl_renderer ALIAS centui_sdl_renderer)
centui_sources(centui_sdl_renderer "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/centui/backends/sdl.hpp")
target_link_libraries(centui_sdl_renderer PUBLIC centui::centui centui::sdl SDL3::SDL3)

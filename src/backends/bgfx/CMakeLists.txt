include(Dependencies.cmake)
add_library(centui_bgfx STATIC)
add_library(centui::bgfx ALIAS centui_bgfx)
centui_sources(centui_bgfx "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/centui/backends/bgfx.hpp")
target_link_libraries(centui_bgfx PUBLIC centui::centui bgfx)

include(Shaders.cmake)
centui_compile_shaders(centui_bgfx)

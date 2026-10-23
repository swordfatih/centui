if(NOT CENTUI_BUILD_SDL)
    return()
endif()

add_executable(centui_example_hud_sdl main.cpp)
target_link_libraries(centui_example_hud_sdl PRIVATE
    centui::centui
    centui::sdl_renderer
)
centui_font_asset(centui_example_hud_sdl)

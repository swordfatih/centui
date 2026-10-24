if(NOT CENTUI_BUILD_SDL OR NOT CENTUI_BUILD_BGFX)
    return()
endif()

add_executable(centui_example_pulse main.cpp)
target_link_libraries(centui_example_pulse PRIVATE
    centui::centui
    centui::sdl
    centui::bgfx
)
centui_font_asset(centui_example_pulse)

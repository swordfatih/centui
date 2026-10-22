if(NOT CENTUI_BUILD_SDL OR NOT CENTUI_BUILD_BGFX)
    return()
endif()

add_executable(centui_example_getting_started main.cpp)
target_link_libraries(centui_example_getting_started PRIVATE
    centui::centui
    centui_config
    centui::sdl
    centui::bgfx
)
centui_font_asset(centui_example_getting_started)

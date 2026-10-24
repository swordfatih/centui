if(NOT CENTUI_BUILD_SDL OR NOT CENTUI_BUILD_BGFX)
    return()
endif()

add_executable(centui_example_custom_primitive main.cpp)
target_link_libraries(centui_example_custom_primitive PRIVATE
    centui::centui
    centui::sdl
    centui::bgfx
)
centui_font_asset(centui_example_custom_primitive)

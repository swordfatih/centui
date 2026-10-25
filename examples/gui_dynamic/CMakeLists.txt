if(NOT CENTUI_BUILD_SDL OR NOT CENTUI_BUILD_BGFX)
    return()
endif()

add_executable(centui_example_gui_dynamic main.cpp ../support/studio.hpp)
target_link_libraries(centui_example_gui_dynamic PRIVATE
    centui::centui
    centui::platform::sdl
    centui::backend::bgfx
)
centui_font_asset(centui_example_gui_dynamic)

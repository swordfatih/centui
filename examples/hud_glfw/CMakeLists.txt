if(NOT CENTUI_BUILD_GLFW OR NOT CENTUI_BUILD_BGFX)
    return()
endif()

add_executable(centui_example_hud_glfw main.cpp)
target_link_libraries(centui_example_hud_glfw PRIVATE
    centui::centui
    centui::platform::glfw
    centui::backend::bgfx
)
centui_font_asset(centui_example_hud_glfw)

if(NOT CENTUI_BUILD_SDL OR NOT CENTUI_BUILD_BGFX)
    return()
endif()

add_executable(centui_example_dashboard main.cpp)
target_link_libraries(centui_example_dashboard PRIVATE
    centui::centui
    centui::platform::sdl
    centui::backend::bgfx
)

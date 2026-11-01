if(NOT CENTUI_BUILD_SDL OR NOT CENTUI_BUILD_BGFX)
    return()
endif()

add_executable(centui_example_task_list main.cpp)
target_link_libraries(centui_example_task_list PRIVATE centui::platform::sdl centui::backend::bgfx)

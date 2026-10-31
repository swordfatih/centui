if(NOT EMSCRIPTEN OR NOT TARGET centui::platform::sdl OR NOT TARGET centui::backend::bgfx)
    return()
endif()

add_executable(centui_example_pulse_web main.cpp)
target_link_libraries(centui_example_pulse_web PRIVATE
    centui::platform::sdl
    centui::backend::bgfx
)
target_link_options(centui_example_pulse_web PRIVATE
    "SHELL:--preload-file \"${PROJECT_SOURCE_DIR}/tests/assets/fonts/Inter-Regular.ttf@/assets/fonts/Inter-Regular.ttf\""
    -sALLOW_MEMORY_GROWTH=1
    -sEXIT_RUNTIME=0
    "SHELL:--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/../web/shell.html\""
)
set_target_properties(centui_example_pulse_web PROPERTIES SUFFIX ".html")
set_property(TARGET centui_example_pulse_web APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../web/shell.html"
)

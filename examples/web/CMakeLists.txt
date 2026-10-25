if(NOT EMSCRIPTEN OR NOT TARGET centui::platform::sdl OR NOT TARGET centui::backend::bgfx)
    return()
endif()

add_executable(centui_example_web main.cpp)
target_link_libraries(centui_example_web PRIVATE
    centui::platform::sdl
    centui::backend::bgfx
)
target_compile_definitions(centui_example_web PRIVATE CENTUI_FONT_PATH="/assets/fonts/Inter-Regular.ttf")
target_link_options(centui_example_web PRIVATE
    -sALLOW_MEMORY_GROWTH=1
    -sEXIT_RUNTIME=0
    "SHELL:--preload-file \"${PROJECT_SOURCE_DIR}/assets/fonts@/assets/fonts\""
    "SHELL:--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/shell.html\""
)
set_target_properties(centui_example_web PROPERTIES SUFFIX ".html")
set_property(TARGET centui_example_web APPEND PROPERTY LINK_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/fonts/Inter-Regular.ttf"
    "${PROJECT_SOURCE_DIR}/assets/fonts/LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)

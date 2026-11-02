if(NOT CENTUI_BUILD_EXAMPLES)
    return()
endif()

add_compile_definitions(CENTUI_FONT_PATH="${PROJECT_SOURCE_DIR}/tests/assets/fonts/Inter-Regular.ttf")

add_subdirectory(getting_started)
add_subdirectory(task_list)
add_subdirectory(overlay)
add_subdirectory(custom_widget)
add_subdirectory(dashboard)

if(NOT CENTUI_BUILD_SFML)
    return()
endif()

add_executable(centui_example_overlay main.cpp)
target_link_libraries(centui_example_overlay PRIVATE
    centui::centui
    centui::backend::sfml
)

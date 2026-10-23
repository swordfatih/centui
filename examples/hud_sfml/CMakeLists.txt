if(NOT CENTUI_BUILD_SFML)
    return()
endif()

add_executable(centui_example_hud_sfml main.cpp)
target_link_libraries(centui_example_hud_sfml PRIVATE
    centui::centui
    centui::sfml
)
centui_font_asset(centui_example_hud_sfml)

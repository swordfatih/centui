include(Dependencies.cmake)
add_library(centui_glfw STATIC)
add_library(centui::glfw ALIAS centui_glfw)
centui_sources(centui_glfw "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/centui/platforms/glfw.hpp")
target_link_libraries(centui_glfw PUBLIC centui::centui glfw)
target_compile_definitions(centui_glfw PRIVATE
    $<$<BOOL:${GLFW_BUILD_X11}>:CENTUI_GLFW_X11>
    $<$<BOOL:${GLFW_BUILD_WAYLAND}>:CENTUI_GLFW_WAYLAND>
)

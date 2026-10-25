if(NOT CENTUI_BUILD_GLFW)
    return()
endif()

include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

add_library(centui_glfw STATIC)
add_library(centui::platform::glfw ALIAS centui_glfw)
centui_sources(centui_glfw "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/centui/platforms/glfw.hpp")
target_link_libraries(centui_glfw PUBLIC centui::centui glfw)
target_compile_definitions(centui_glfw PRIVATE
    $<$<BOOL:${GLFW_BUILD_X11}>:CENTUI_GLFW_X11>
    $<$<BOOL:${GLFW_BUILD_WAYLAND}>:CENTUI_GLFW_WAYLAND>
)

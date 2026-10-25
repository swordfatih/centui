if(NOT CENTUI_BUILD_BGFX)
    return()
endif()

include(FetchContent)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLE_COMMON OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)

FetchContent_Declare(bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG v1.162.9524-580
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(bgfx_cmake)

add_library(centui_bgfx STATIC)
add_library(centui::backend::bgfx ALIAS centui_bgfx)
centui_sources(centui_bgfx "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/centui/backends/bgfx.hpp")
target_link_libraries(centui_bgfx PUBLIC centui::centui bgfx)

if(MSVC)
    target_compile_options(centui_bgfx PRIVATE /Zc:__cplusplus)
endif()

include(Shaders.cmake)
centui_compile_shaders(centui_bgfx)

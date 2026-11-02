if(NOT CENTUI_BUILD_BGFX)
    return()
endif()

include(FetchContent)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "")
set(BGFX_BUILD_TESTS OFF CACHE BOOL "")
set(BGFX_BUILD_EXAMPLE_COMMON OFF CACHE BOOL "")
set(BGFX_INSTALL OFF CACHE BOOL "")

set(CENTUI_BGFX_SHADERC "" CACHE FILEPATH "Host shaderc executable from the bgfx revision used by CentUI")

if(CENTUI_BGFX_SHADERC AND NOT TARGET bgfx::shaderc)
    if(NOT IS_ABSOLUTE "${CENTUI_BGFX_SHADERC}" OR NOT EXISTS "${CENTUI_BGFX_SHADERC}" OR IS_DIRECTORY "${CENTUI_BGFX_SHADERC}")
        message(FATAL_ERROR "CENTUI_BGFX_SHADERC must name an existing absolute path to a host shaderc executable")
    endif()

    add_executable(bgfx::shaderc IMPORTED GLOBAL)
    set_target_properties(bgfx::shaderc PROPERTIES IMPORTED_LOCATION "${CENTUI_BGFX_SHADERC}")
endif()

if(TARGET bgfx::shaderc)
    # Reuse host tooling instead of compiling tools for the target device.
    set(BGFX_BUILD_TOOLS OFF)
elseif(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "Cross-compiling CentUI's bgfx backend requires a host shaderc. "
        "Set CENTUI_BGFX_SHADERC to its absolute path or provide a bgfx::shaderc target."
    )
else()
    set(BGFX_BUILD_TOOLS ON)
endif()

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
    target_compile_options(centui_bgfx PRIVATE /Zc:__cplusplus /Zc:preprocessor)
endif()

include(Shaders.cmake)
centui_compile_shaders(centui_bgfx)

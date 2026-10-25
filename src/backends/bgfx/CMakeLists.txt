if(NOT CENTUI_BUILD_BGFX)
    return()
endif()

include(FetchContent)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLE_COMMON OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)

if(EMSCRIPTEN)
    set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_MULTITHREADED OFF CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_VIDEO OFF CACHE BOOL "" FORCE)
    set(BGFX_OPENGLES_VERSION 30 CACHE STRING "" FORCE)

    if(NOT TARGET bgfx::shaderc)
        find_program(CENTUI_BGFX_SHADERC NAMES shaderc
            DOC "Host shaderc executable from the same bgfx revision used by CentUI"
            NO_CMAKE_FIND_ROOT_PATH
        )

        if(NOT CENTUI_BGFX_SHADERC OR NOT EXISTS "${CENTUI_BGFX_SHADERC}")
            message(FATAL_ERROR
                "Emscripten builds require a host shaderc. Set CENTUI_BGFX_SHADERC to its absolute path."
            )
        endif()

        add_executable(bgfx::shaderc IMPORTED GLOBAL)
        set_target_properties(bgfx::shaderc PROPERTIES IMPORTED_LOCATION "${CENTUI_BGFX_SHADERC}")
    endif()
else()
    set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
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

if(EMSCRIPTEN)
    target_link_options(centui_bgfx PUBLIC
        -sMIN_WEBGL_VERSION=2
        -sMAX_WEBGL_VERSION=2
        -sGL_ENABLE_GET_PROC_ADDRESS=1
    )
endif()

if(MSVC)
    target_compile_options(centui_bgfx PRIVATE /Zc:__cplusplus /Zc:preprocessor)
endif()

include(Shaders.cmake)
centui_compile_shaders(centui_bgfx)

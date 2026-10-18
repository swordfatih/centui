cmake_minimum_required(VERSION 4.3)

# Enable experimental standard library modules (CMake 4.3)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(centui LANGUAGES C CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_MODULE_STD ON)

# ------------------------
# Options
# ------------------------
function(project_option name description default)
    string(TOUPPER "${PROJECT_NAME}" prefix)
    option(${prefix}_${name} "${description}" ${default})
    set(${name} "${${prefix}_${name}}" PARENT_SCOPE)
endfunction()

project_option(BUILD_EXAMPLES "Build all example executables" ON)
project_option(BUILD_TESTS "Build the test executable" ON)

option(CENTUI_BUILD_BGFX "Build the bgfx backend and shaders" ON)
option(CENTUI_BUILD_SFML "Build the SFML backend" ON)
option(CENTUI_BUILD_APP "Build the SDL3/bgfx application runner" ${CENTUI_BUILD_BGFX})
if(CENTUI_BUILD_APP AND NOT CENTUI_BUILD_BGFX)
    message(FATAL_ERROR "CENTUI_BUILD_APP requires CENTUI_BUILD_BGFX")
endif()

include(FetchContent)

if(BUILD_TESTS)
    enable_testing()
endif()

# ------------------------
# Configuration
# ------------------------
add_library(${PROJECT_NAME}_config INTERFACE)

target_compile_options(${PROJECT_NAME}_config INTERFACE -Wall -Wextra -Wpedantic -Wno-unused-but-set-variable -Wno-unused-parameter)

if(MINGW)
    target_link_options(${PROJECT_NAME}_config INTERFACE -static -static-libgcc -static-libstdc++)
endif()

# Keep module sets explicit so enabling one backend cannot pull in another.
function(centui_module_library target alias)
    add_library(${target} STATIC)
    add_library(${alias} ALIAS ${target})
    target_sources(${target} PUBLIC FILE_SET CXX_MODULES
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${ARGN})
    target_compile_features(${target} PUBLIC cxx_std_26)
    target_compile_options(${target} PUBLIC -freflection)
    set_target_properties(${target} PROPERTIES CXX_MODULE_STD ON)
    target_link_libraries(${target} PRIVATE ${PROJECT_NAME}_config)
endfunction()

include(src/core/Dependencies.cmake)
centui_module_library(centui_values centui::values
    "${PROJECT_SOURCE_DIR}/src/core/unit.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/color.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/viewport.cppm")
centui_module_library(centui_input centui::input
    "${PROJECT_SOURCE_DIR}/src/core/input.cppm")
target_link_libraries(centui_input PUBLIC centui_values)
centui_module_library(centui_glyph centui::glyph
    "${PROJECT_SOURCE_DIR}/src/core/glyph.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/font.cppm")
target_link_libraries(centui_glyph PUBLIC centui_values PRIVATE freetype harfbuzz)
centui_module_library(centui_core centui::core
    "${PROJECT_SOURCE_DIR}/src/core/style.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/layout.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/node.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/draw.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/element.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/container.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/text.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/button.cppm"
    "${PROJECT_SOURCE_DIR}/src/core/ui.cppm")
target_link_libraries(centui_core PUBLIC centui_glyph centui_input PRIVATE yogacore)

if(CENTUI_BUILD_BGFX)
    add_subdirectory(src/backends/bgfx)
    add_subdirectory(src/app)
endif()
if(CENTUI_BUILD_SFML)
    add_subdirectory(src/backends/sfml)
endif()

if(BUILD_EXAMPLES)
    if(CENTUI_BUILD_APP)
        add_executable(centui_example_app examples/main.cpp)
        target_link_libraries(centui_example_app PRIVATE centui::app ${PROJECT_NAME}_config)
    endif()
    if(CENTUI_BUILD_SFML)
        add_executable(centui_example_sfml examples/sfml.cpp)
        target_link_libraries(centui_example_sfml PRIVATE centui::sfml ${PROJECT_NAME}_config)
    endif()
    add_executable(centui_example_custom_backend examples/custom_backend.cpp)
    target_link_libraries(centui_example_custom_backend PRIVATE centui::core ${PROJECT_NAME}_config)
endif()

if(BUILD_TESTS)
    add_executable(centui_tests tests/font.test.cpp tests/node.test.cpp tests/style.test.cpp)
    target_link_libraries(centui_tests PRIVATE centui::core ${PROJECT_NAME}_config Catch2::Catch2WithMain)
    include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(centui_tests)
endif()

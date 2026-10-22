cmake_minimum_required(VERSION 4.3)

project(centui LANGUAGES C CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

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
option(CENTUI_BUILD_SDL "Build the SDL platform" ON)

include(FetchContent)

if(BUILD_TESTS)
    enable_testing()
endif()

# ------------------------
# Configuration
# ------------------------
add_library(${PROJECT_NAME}_config INTERFACE)
target_compile_options(${PROJECT_NAME}_config INTERFACE -Wall -Wextra -Wpedantic -Wno-unused-but-set-variable -Wno-unused-parameter)

# ------------------------
# Library
# ------------------------
add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_26)
target_compile_options(${PROJECT_NAME} PUBLIC -freflection)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_config)

function(centui_sources target directory)
    file(GLOB_RECURSE cpp_sources CONFIGURE_DEPENDS "${directory}/*.cpp")
    file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS "${directory}/*.cppm")
    target_sources(${target}
        PRIVATE ${cpp_sources}
        PUBLIC FILE_SET CXX_MODULES
            BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${module_sources})
endfunction()

include(src/core/Dependencies.cmake)
centui_sources(centui "${PROJECT_SOURCE_DIR}/src/core")
target_link_libraries(${PROJECT_NAME} PRIVATE yogacore freetype harfbuzz)

if(CENTUI_BUILD_BGFX)
    add_subdirectory(src/backends/bgfx)
endif()
if(CENTUI_BUILD_SFML)
    add_subdirectory(src/platforms/sfml)
    add_subdirectory(src/backends/sfml)
endif()
if(CENTUI_BUILD_SDL)
    add_subdirectory(src/platforms/sdl)
endif()

# ------------------------
# Examples
# ------------------------
function(centui_font_asset target)
    # Font and license from https://github.com/rsms/inter/releases/tag/v4.1.
    set(font_directory "$<TARGET_FILE_DIR:${target}>/assets/fonts")
    target_compile_definitions(${target} PRIVATE CENTUI_FONT_PATH="${font_directory}/Inter-Regular.ttf")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${font_directory}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/assets/fonts/Inter-Regular.ttf"
            "${PROJECT_SOURCE_DIR}/assets/fonts/LICENSE.txt"
            "${font_directory}"
        VERBATIM)
endfunction()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ------------------------
# Tests
# ------------------------
if(BUILD_TESTS)
    file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/*.cpp")
    if(test_sources)
        set(test_target "${PROJECT_NAME}_tests")
        add_executable(${test_target} ${test_sources})
        centui_font_asset(${test_target})
        target_link_libraries(${test_target} PRIVATE ${PROJECT_NAME}::${PROJECT_NAME} ${PROJECT_NAME}_config Catch2::Catch2WithMain)
        include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
        catch_discover_tests(${test_target})
    endif()
endif()

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

# ------------------------
# Library
# ------------------------
add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_26)
target_compile_options(${PROJECT_NAME} PUBLIC -freflection)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_config)

function(centui_sources directory)
    file(GLOB_RECURSE cpp_sources CONFIGURE_DEPENDS "${directory}/*.cpp")
    file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS "${directory}/*.cppm")
    target_sources(${PROJECT_NAME}
        PRIVATE ${cpp_sources}
        PUBLIC FILE_SET CXX_MODULES
            BASE_DIRS "${PROJECT_SOURCE_DIR}/src" FILES ${module_sources})
endfunction()

include(src/core/Dependencies.cmake)
centui_sources("${PROJECT_SOURCE_DIR}/src/core")
target_link_libraries(${PROJECT_NAME} PRIVATE yogacore freetype harfbuzz)

if(CENTUI_BUILD_BGFX)
    add_subdirectory(src/backends/bgfx)
endif()
if(CENTUI_BUILD_SFML)
    add_subdirectory(src/backends/sfml)
endif()
if(CENTUI_BUILD_APP)
    add_subdirectory(src/app)
endif()

# ------------------------
# Examples
# ------------------------
if(BUILD_EXAMPLES)
    file(GLOB example_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/*.cpp")
    foreach(example_source IN LISTS example_sources)
        get_filename_component(example_name "${example_source}" NAME_WE)
        if(example_name STREQUAL "main")
            if(NOT CENTUI_BUILD_APP)
                continue()
            endif()
            set(example_name app)
        elseif(example_name STREQUAL "sfml" AND NOT CENTUI_BUILD_SFML)
            continue()
        endif()
        set(example_target "${PROJECT_NAME}_example_${example_name}")
        add_executable(${example_target} "${example_source}")
        target_link_libraries(${example_target} PRIVATE ${PROJECT_NAME}::${PROJECT_NAME} ${PROJECT_NAME}_config)
    endforeach()
endif()

# ------------------------
# Tests
# ------------------------
if(BUILD_TESTS)
    file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/*.cpp")
    if(test_sources)
        set(test_target "${PROJECT_NAME}_tests")
        add_executable(${test_target} ${test_sources})
        target_link_libraries(${test_target} PRIVATE ${PROJECT_NAME}::${PROJECT_NAME} ${PROJECT_NAME}_config Catch2::Catch2WithMain)
        include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
        catch_discover_tests(${test_target})
    endif()
endif()

cmake_minimum_required(VERSION 4.3)

# Enable experimental standard library modules (CMake 4.3)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(starter LANGUAGES CXX)

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
file(GLOB_RECURSE cpp_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cppm")

add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE ${cpp_sources}
    PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules" FILES ${module_sources}
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_26)
target_compile_options(${PROJECT_NAME} PUBLIC -freflection)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_MODULE_STD ON)

target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

target_link_libraries(${PROJECT_NAME}
    PRIVATE ${PROJECT_NAME}_config
    # add dependencies here
)

# ------------------------
# Examples
# ------------------------
if(BUILD_EXAMPLES)
    FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.x GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(spdlog)

    file(GLOB example_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")

    foreach(example_source IN LISTS example_sources)
        get_filename_component(example_name "${example_source}" NAME_WE)
        set(example_target "${PROJECT_NAME}_${example_name}")
        add_executable(${example_target} "${example_source}")
        target_link_libraries(${example_target} PRIVATE ${PROJECT_NAME}::${PROJECT_NAME} ${PROJECT_NAME}_config spdlog::spdlog)
    endforeach()
endif()

# ------------------------
# Tests
# ------------------------
if(BUILD_TESTS)
    FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.14.0 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)

    file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

    if(test_sources)
        set(test_target "${PROJECT_NAME}_tests")
        add_executable(${test_target} ${test_sources})
        target_link_libraries(${test_target} PRIVATE ${PROJECT_NAME}::${PROJECT_NAME} ${PROJECT_NAME}_config Catch2::Catch2WithMain)
        include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
        catch_discover_tests(${test_target})
    endif()
endif()

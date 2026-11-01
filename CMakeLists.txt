cmake_minimum_required(VERSION 3.28)

project(centui LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# ------------------------
# Options
# ------------------------
option(CENTUI_BUILD_EXAMPLES "Build enabled example executables" OFF)
option(CENTUI_BUILD_TESTS "Build unit tests and integrations for enabled adapters" OFF)

option(CENTUI_BUILD_BGFX "Build the bgfx backend and shaders" OFF)
option(CENTUI_BUILD_SFML "Build the SFML platform and renderer backend" OFF)
option(CENTUI_BUILD_SDL "Build the SDL platform and renderer backend" OFF)
option(CENTUI_BUILD_GLFW "Build the GLFW platform" OFF)

include(FetchContent)

if(CENTUI_BUILD_TESTS)
    enable_testing()
endif()

# ------------------------
# Library
# ------------------------
add_library(centui STATIC)
add_library(centui::centui ALIAS centui)

target_compile_features(centui PUBLIC cxx_std_20)

function(centui_sources target directory header_pattern)
    file(GLOB_RECURSE cpp_sources CONFIGURE_DEPENDS "${directory}/*.cpp")
    file(GLOB_RECURSE hpp_sources CONFIGURE_DEPENDS "${header_pattern}")
    target_sources(${target} PRIVATE ${cpp_sources} ${hpp_sources})
    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
endfunction()

include(src/centui/Dependencies.cmake)
add_subdirectory(src/centypo)
centui_sources(centui "${PROJECT_SOURCE_DIR}/src/centui" "${PROJECT_SOURCE_DIR}/include/centui/*.hpp")
target_link_libraries(centui PUBLIC centypo::centypo PRIVATE yogacore lunasvg::lunasvg plutovg::plutovg)

add_subdirectory(src/platforms/sfml)
add_subdirectory(src/platforms/sdl)
add_subdirectory(src/platforms/glfw)

add_subdirectory(src/backends/bgfx)
add_subdirectory(src/backends/sfml)
add_subdirectory(src/backends/sdl)

# ------------------------
# Examples
# ------------------------
add_subdirectory(tests/examples)

# ------------------------
# Tests
# ------------------------
add_subdirectory(tests)

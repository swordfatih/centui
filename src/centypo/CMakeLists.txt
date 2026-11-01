cmake_minimum_required(VERSION 3.28)

project(centypo LANGUAGES C CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(CENTYPO_SYSTEM_FONTS "Enable native system font discovery" ON)

add_library(centypo STATIC)
add_library(centypo::centypo ALIAS centypo)
target_compile_features(centypo PUBLIC cxx_std_20)
target_include_directories(centypo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<INSTALL_INTERFACE:include>)

include(Dependencies.cmake)

file(GLOB sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
target_sources(centypo PRIVATE ${sources})
target_link_libraries(centypo PRIVATE freetype harfbuzz centypo_bidi libunibreak)

add_subdirectory(system)

if(EMSCRIPTEN)
    target_compile_options(centypo PUBLIC -fexceptions)
    target_link_options(centypo PUBLIC -fexceptions)
endif()


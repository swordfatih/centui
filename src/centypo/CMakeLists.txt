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

if(CENTYPO_SYSTEM_FONTS AND NOT EMSCRIPTEN)
    if(WIN32)
        target_link_libraries(centypo PRIVATE dwrite)
    elseif(APPLE)
        target_link_libraries(centypo PRIVATE "-framework CoreText" "-framework CoreFoundation")
    elseif(NOT ANDROID)
        find_package(Fontconfig 2.12 REQUIRED)

        target_link_libraries(centypo PRIVATE Fontconfig::Fontconfig)
    endif()
else()
    target_compile_definitions(centypo PRIVATE CENTYPO_DISABLE_SYSTEM_FONTS)
endif()

if(EMSCRIPTEN)
    target_compile_options(centypo PUBLIC -fexceptions)
    target_link_options(centypo PUBLIC -fexceptions)
endif()


cmake_minimum_required(VERSION 3.28)

project(centui_consumer LANGUAGES C CXX)

set(CENTUI_SOURCE "" CACHE PATH "CentUI checkout")

set(CONSUMER_MODE "fetchcontent" CACHE STRING "fetchcontent or subdirectory")

if(CONSUMER_MODE STREQUAL "fetchcontent")
    include(FetchContent)
    FetchContent_Declare(centui GIT_REPOSITORY "${CENTUI_SOURCE}" GIT_TAG HEAD)
    FetchContent_MakeAvailable(centui)
elseif(CONSUMER_MODE STREQUAL "subdirectory")
    add_subdirectory("${CENTUI_SOURCE}" centui)
else()
    message(FATAL_ERROR "Unknown consumer mode")
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE centui::centui)

if(EMSCRIPTEN)
    target_link_options(consumer PRIVATE "SHELL:--preload-file \"${CENTUI_SOURCE}/tests/assets/fonts/Inter-Regular.ttf@/assets/fonts/Inter-Regular.ttf\"")
    set(consumer_font_path "/assets/fonts/Inter-Regular.ttf")
else()
    set(consumer_font_path "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts/Inter-Regular.ttf")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../assets/fonts/Inter-Regular.ttf" "${consumer_font_path}" COPYONLY)
endif()
target_compile_definitions(consumer PRIVATE CENTUI_FONT_PATH="${consumer_font_path}")

enable_testing()

add_test(NAME consumer COMMAND consumer)

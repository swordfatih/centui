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

enable_testing()

add_test(NAME consumer COMMAND consumer)

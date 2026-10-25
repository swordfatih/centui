if(NOT CENTUI_BUILD_TESTS)
    return()
endif()

include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.14.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

file(GLOB test_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/unit/*.test.cpp")
if(test_sources)
    set(test_target "centui_tests")
    add_executable(${test_target} ${test_sources})
    centui_font_asset(${test_target})
    target_link_libraries(${test_target} PRIVATE centui::centui Catch2::Catch2WithMain)
    include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(${test_target})
endif()

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

target_compile_features(Catch2 PUBLIC cxx_std_20)

file(GLOB test_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/unit/*.test.cpp")
if(test_sources)
    set(test_target "centui_tests")
    add_executable(${test_target} ${test_sources})
    centui_font_asset(${test_target})
    target_link_libraries(${test_target} PRIVATE centui::centui Catch2::Catch2WithMain)
    target_link_libraries(${test_target} PRIVATE centypo_bidi freetype)
    target_include_directories(${test_target} PRIVATE "${PROJECT_SOURCE_DIR}/src/centui" "${PROJECT_SOURCE_DIR}/src/centypo")
    FetchContent_GetProperties(libunibreak SOURCE_DIR libunibreak_SOURCE_DIR)
    FetchContent_GetProperties(centypo_sheenbidi SOURCE_DIR sheenbidi_SOURCE_DIR)
    target_compile_definitions(${test_target} PRIVATE
        CENTUI_GRAPHEME_TEST_PATH="${libunibreak_SOURCE_DIR}/src/GraphemeBreakTest.txt"
        CENTUI_WORD_TEST_PATH="${libunibreak_SOURCE_DIR}/src/WordBreakTest.txt")
    target_compile_definitions(${test_target} PRIVATE
        CENTUI_BIDI_TEST_PATH="${sheenbidi_SOURCE_DIR}/Tools/Unicode/BidiCharacterTest.txt")
    include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(${test_target})
endif()

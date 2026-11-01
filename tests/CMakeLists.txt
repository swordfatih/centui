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
include(${catch2_SOURCE_DIR}/extras/Catch.cmake)

function(centui_test target sources)
    add_executable(${target} ${sources})
    target_link_libraries(${target} PRIVATE Catch2::Catch2WithMain)
    catch_discover_tests(${target} ${ARGN})
endfunction()

add_subdirectory(unit)
add_subdirectory(integration)

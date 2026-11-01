if(NOT CENTUI_BUILD_SFML)
    return()
endif()

if(NOT TARGET SFML::Graphics)
    include(FetchContent)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "")
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "")
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "")
    set(SFML_INSTALL_PKGCONFIG_FILES OFF CACHE BOOL "")
    FetchContent_Declare(sfml
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.1.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(sfml)
endif()

add_library(centui_sfml_platform STATIC)
add_library(centui::platform::sfml ALIAS centui_sfml_platform)
centui_sources(centui_sfml_platform "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/centui/platforms/sfml.hpp")
target_link_libraries(centui_sfml_platform PUBLIC centui::centui SFML::Graphics)

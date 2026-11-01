if(NOT CENTUI_BUILD_SDL)
    return()
endif()

if(NOT TARGET SDL3::SDL3)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "")
    set(SDL_STATIC ON CACHE BOOL "")
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "")
    set(SDL_TESTS OFF CACHE BOOL "")
    set(SDL_EXAMPLES OFF CACHE BOOL "")
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "")
    FetchContent_Declare(sdl3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.18
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(sdl3)
endif()

add_library(centui_sdl STATIC)
add_library(centui::platform::sdl ALIAS centui_sdl)
centui_sources(centui_sdl "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/centui/platforms/sdl.hpp")
target_link_libraries(centui_sdl PUBLIC centui::centui SDL3::SDL3)

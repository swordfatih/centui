
include_guard(GLOBAL)
include(FetchContent)

# ------------------------
# SDL3
# ------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE
)

# ------------------------
# Yoga
# ------------------------
FetchContent_Declare(yoga
    GIT_REPOSITORY https://github.com/facebook/yoga.git
    GIT_TAG v3.2.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR yoga
)

# ------------------------
# bgfx + bx + bimg
# ------------------------
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLE_COMMON OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG v1.162.9524-580
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
)

FetchContent_MakeAvailable(
    sdl3
    yoga
    bgfx_cmake
)

# ------------------------
# Examples: spdlog
# ------------------------
if(BUILD_EXAMPLES)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.x
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(spdlog)
endif()

# ------------------------
# Tests: Catch2
# ------------------------
if(BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.14.0
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(Catch2)
endif()

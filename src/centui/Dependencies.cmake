include_guard(GLOBAL)
include(FetchContent)

function(centui_dependencies)
    if(EMSCRIPTEN)
        set(CMAKE_DISABLE_FIND_PACKAGE_Threads ON)
        set(CMAKE_USE_PTHREADS_INIT FALSE)
    endif()

    FetchContent_Declare(yoga
        GIT_REPOSITORY https://github.com/facebook/yoga.git
        GIT_TAG v3.2.1
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR yoga
    )
    FetchContent_MakeAvailable(yoga)

    # LunaSVG 3.5.0 bundles PlutoVG 1.3.1 and its embedded raster decoder.
    set(LUNASVG_BUILD_EXAMPLES OFF)
    set(PLUTOVG_BUILD_EXAMPLES OFF)
    set(USE_SYSTEM_PLUTOVG OFF)
    # SVG text uses LunaSVG's system-font lookup, independently of CentUI text styles.
    set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS OFF)
    FetchContent_Declare(lunasvg
        GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
        GIT_TAG v3.5.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(lunasvg)

endfunction()

centui_dependencies()

include_guard(GLOBAL)
include(FetchContent)

function(centui_dependencies)
    FetchContent_Declare(yoga
        GIT_REPOSITORY https://github.com/facebook/yoga.git
        GIT_TAG v3.2.1
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR yoga
    )
    FetchContent_MakeAvailable(yoga)

    if(APPLE)
        # Yoga's -fno-rtti with exceptions can break std::exception matching in
        # the linked application: https://github.com/llvm/llvm-project/issues/66117
        target_compile_options(yogacore PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
    endif()

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

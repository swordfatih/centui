include_guard(GLOBAL)
include(FetchContent)

FetchContent_Declare(yoga
    GIT_REPOSITORY https://github.com/facebook/yoga.git
    GIT_TAG v3.2.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR yoga
)

set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
FetchContent_Declare(freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-14-2
    GIT_SHALLOW TRUE
)

set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
set(HB_BUILD_RASTER OFF CACHE BOOL "" FORCE)
set(HB_BUILD_VECTOR OFF CACHE BOOL "" FORCE)
set(HB_BUILD_GPU OFF CACHE BOOL "" FORCE)
set(HB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(harfbuzz
    GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
    GIT_TAG 14.5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(yoga freetype harfbuzz)

if(BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.14.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
endif()

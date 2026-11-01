include(FetchContent)

function(centypo_dependencies)
    if(NOT TARGET freetype)
        set(FT_DISABLE_HARFBUZZ ON)
        set(FT_DISABLE_BZIP2 ON)
        set(FT_DISABLE_BROTLI ON)
        set(FT_DISABLE_PNG ON)

        FetchContent_Declare(freetype
            GIT_REPOSITORY https://github.com/freetype/freetype.git
            GIT_TAG VER-2-14-2
            GIT_SHALLOW TRUE)

        FetchContent_MakeAvailable(freetype)
    endif()

    if(NOT TARGET harfbuzz)
        set(HB_HAVE_FREETYPE ON)
        set(HB_BUILD_UTILS OFF)
        set(HB_BUILD_SUBSET OFF)
        set(HB_BUILD_RASTER OFF)
        set(HB_BUILD_VECTOR OFF)
        set(HB_BUILD_GPU OFF)
        set(HB_BUILD_TESTS OFF)

        FetchContent_Declare(harfbuzz
            GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
            GIT_TAG 14.5.0
            GIT_SHALLOW TRUE)

        FetchContent_MakeAvailable(harfbuzz)
    endif()
endfunction()

centypo_dependencies()

FetchContent_Declare(centypo_sheenbidi
    GIT_REPOSITORY https://github.com/Tehreer/SheenBidi.git
    GIT_TAG 9c048a32d2131f67608e85ccccf9079cc46d6e42
    SOURCE_SUBDIR Source)

FetchContent_Declare(libunibreak
    GIT_REPOSITORY https://github.com/adah1972/libunibreak.git
    GIT_TAG f8172fe771ef620112247cadfb876aecaa4f769e
    SOURCE_SUBDIR src)

FetchContent_MakeAvailable(centypo_sheenbidi libunibreak)

add_library(centypo_bidi STATIC "${centypo_sheenbidi_SOURCE_DIR}/Source/SheenBidi.c")
target_compile_features(centypo_bidi PRIVATE c_std_11)
target_compile_definitions(centypo_bidi PRIVATE SB_CONFIG_UNITY)
target_include_directories(centypo_bidi
    PUBLIC "${centypo_sheenbidi_SOURCE_DIR}/Headers"
    PRIVATE "${centypo_sheenbidi_SOURCE_DIR}/Source")

file(GLOB unibreak_sources CONFIGURE_DEPENDS "${libunibreak_SOURCE_DIR}/src/*.c")
list(FILTER unibreak_sources EXCLUDE REGEX "/tests\\.c$")

# Other data files are included by implementation files.
list(FILTER unibreak_sources EXCLUDE REGEX "/[^/]*data\\.c$")
list(APPEND unibreak_sources "${libunibreak_SOURCE_DIR}/src/linebreakdata.c")

add_library(libunibreak STATIC ${unibreak_sources})
target_include_directories(libunibreak PUBLIC "${libunibreak_SOURCE_DIR}/src")

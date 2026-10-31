#include "glyph_image.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Native BGRA rows become straight RGBA and honor negative pitch", "[font][raster]")
{
    unsigned char pixels[]{0, 0, 255, 255, 255, 0, 0, 255, 0, 64, 128, 128, 0, 0, 0, 0};
    FT_Bitmap source{};
    source.width = 2;
    source.rows = 2;
    source.pitch = -8;
    source.pixel_mode = FT_PIXEL_MODE_BGRA;
    source.buffer = pixels + 8;

    const auto image = centypo::details::copy_glyph_bitmap(source, -2, 9);
    REQUIRE(image.format == centypo::pixel_format::rgba);
    REQUIRE(image.left == -2);
    REQUIRE(image.top == 9);
    REQUIRE(image.pixels == std::vector<std::uint8_t>{255, 128, 0, 128, 0, 0, 0, 0, 255, 0, 0, 255, 0, 0, 255, 255});

    source.pitch = 1;
    REQUIRE_THROWS_AS(centypo::details::copy_glyph_bitmap(source, 0, 0), std::invalid_argument);
}

TEST_CASE("Mono bitmaps and reduced gray ranges become tintable masks", "[font][raster]")
{
    unsigned char mono[]{0x80, 0x80};
    FT_Bitmap source{};
    source.width = 9;
    source.rows = 1;
    source.pitch = 2;
    source.pixel_mode = FT_PIXEL_MODE_MONO;
    source.buffer = mono;

    const auto image = centypo::details::copy_glyph_bitmap(source, 0, 0);
    REQUIRE(image.format == centypo::pixel_format::alpha);
    REQUIRE(image.pixels == std::vector<std::uint8_t>{255, 0, 0, 0, 0, 0, 0, 0, 255});

    unsigned char gray[]{0, 15};
    source.width = 2;
    source.buffer = gray;
    source.num_grays = 16;
    source.pixel_mode = FT_PIXEL_MODE_GRAY;

    REQUIRE(centypo::details::copy_glyph_bitmap(source, 0, 0).pixels == std::vector<std::uint8_t>{0, 255});
}

TEST_CASE("Glyph conversion handles padding and degenerate color ranges", "[font][raster]")
{
    FT_Bitmap source{};
    source.width = 2;
    source.rows = 1;

    SECTION("Transparent color clears RGB and unpremultiplied channels saturate")
    {
        unsigned char pixels[]{100, 150, 200, 0, 255, 128, 64, 128};
        source.pitch = 8;
        source.pixel_mode = FT_PIXEL_MODE_BGRA;
        source.buffer = pixels;

        const auto image = centypo::details::copy_glyph_bitmap(source, 0, 0);

        REQUIRE(image.pixels == std::vector<std::uint8_t>{0, 0, 0, 0, 128, 255, 255, 128});
    }

    SECTION("Gray row padding is excluded from the output")
    {
        unsigned char pixels[]{0, 15, 99, 99, 7, 14, 99, 99};
        source.rows = 2;
        source.pitch = 4;
        source.pixel_mode = FT_PIXEL_MODE_GRAY;
        source.num_grays = 16;
        source.buffer = pixels;

        const auto image = centypo::details::copy_glyph_bitmap(source, 0, 0);

        REQUIRE(image.pixels == std::vector<std::uint8_t>{0, 255, 119, 238});
    }

    SECTION("A single gray level produces a transparent mask")
    {
        unsigned char pixels[]{0, 255};
        source.pitch = 2;
        source.pixel_mode = FT_PIXEL_MODE_GRAY;
        source.num_grays = 1;
        source.buffer = pixels;

        REQUIRE(centypo::details::copy_glyph_bitmap(source, 0, 0).pixels == std::vector<std::uint8_t>{0, 0});
    }

    SECTION("Empty bitmaps require no buffer or supported pixel format")
    {
        source.width = 0;
        source.pixel_mode = FT_PIXEL_MODE_NONE;

        REQUIRE(centypo::details::copy_glyph_bitmap(source, 0, 0).pixels.empty());
    }
}

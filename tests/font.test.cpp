
#include <catch2/catch_test_macros.hpp>

import std;
import centui.font;

TEST_CASE("Measure system font", "[font]")
{
    centui::font font;

    const auto short_text = font.measure("Hello", 16);
    const auto long_text = font.measure("Hello world", 16);

    REQUIRE(short_text.width > 0);
    REQUIRE(short_text.height > 0);
    REQUIRE(long_text.width > short_text.width);
}

TEST_CASE("Measure text at different sizes", "[font]")
{
    centui::font font;

    const auto small = font.measure("Hello", 12);
    const auto large = font.measure("Hello", 24);

    REQUIRE(large.width > small.width);
    REQUIRE(large.height > small.height);
}

TEST_CASE("Text layout supplies measurement and glyph positions", "[font]")
{
    centui::font face;

    for(const auto text: {"", "Hello", "office", "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7"})
    {
        const auto layout = face.layout_text(text, 19.5f);
        const auto measured = face.measure(text, 19.5f);
        const auto shaped = face.shape(text, 19.5f);

        REQUIRE(layout.bounds.width == measured.width);
        REQUIRE(layout.bounds.height == measured.height);
        REQUIRE(layout.ascender == face.ascender(19.5f));
        REQUIRE(layout.glyphs.size() == shaped.size());

        for(std::size_t i = 0; i < shaped.size(); ++i)
        {
            REQUIRE(layout.glyphs[i].id == shaped[i].id);
            REQUIRE(layout.glyphs[i].x == shaped[i].x);
            REQUIRE(layout.glyphs[i].y == shaped[i].y);
        }
    }
}

TEST_CASE("Font operations reject invalid sizes", "[font]")
{
    centui::font face;

    for(const auto size: {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::max()})
    {
        REQUIRE_THROWS_AS(face.layout_text("Hello", size), std::invalid_argument);
        REQUIRE_THROWS_AS(face.measure("Hello", size), std::invalid_argument);
        REQUIRE_THROWS_AS(face.shape("Hello", size), std::invalid_argument);
        REQUIRE_THROWS_AS(face.ascender(size), std::invalid_argument);
        REQUIRE_THROWS_AS(face.rasterize(0, size), std::invalid_argument);
    }
}

TEST_CASE("Font resources remain usable after ownership transfer", "[font]")
{
    centui::font original;
    const auto expected = original.measure("Hello", 16);
    auto moved = std::move(original);
    REQUIRE(moved.measure("Hello", 16).width == expected.width);
    centui::font replacement;
    replacement = std::move(moved);
    REQUIRE(replacement.measure("Hello", 16).width == expected.width);
    REQUIRE_THROWS_AS(centui::font(std::filesystem::path{"centui-missing-font-for-test.ttf"}), std::runtime_error);
}

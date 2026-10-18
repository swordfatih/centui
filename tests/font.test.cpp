
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

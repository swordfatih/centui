#include <catch2/catch_test_macros.hpp>

import std;
import centui.style;

struct my_style
{
    int gap = 20;
    int padding = 12;
};

struct my_second_style
{
    int gap = 30;
};

TEST_CASE("Resolve style from struct", "[style]")
{
    constexpr auto style = centui::style::resolve<my_style>();

    REQUIRE(style.gap == 20);
    REQUIRE(style.padding == 12);
}

TEST_CASE("Resolve multiple styles", "[style]")
{
    constexpr auto style = centui::style::resolve<my_style, my_second_style>();

    REQUIRE(style.gap == 30);
    REQUIRE(style.padding == 12);
}

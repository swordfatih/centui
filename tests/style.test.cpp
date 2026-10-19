#include <catch2/catch_test_macros.hpp>

import centui.style;
import centui.unit;

struct my_style
{
    centui::length gap = centui::px(20);
    centui::edges  padding = centui::edges::all(centui::px(20));
};

struct my_second_style
{
    centui::length   gap = centui::px(30);
    centui::position position = centui::position::absolute;

    struct
    {
        centui::length left = centui::px(40);
    } padding;
};

TEST_CASE("Resolve style from struct", "[style]")
{
    constexpr auto style = centui::style::resolve<my_style>();

    REQUIRE(style.gap == centui::length{20, centui::unit::px});
    REQUIRE(style.padding == centui::edges::all(centui::px(20)));
}

TEST_CASE("Resolve multiple styles", "[style]")
{
    constexpr auto style = centui::style::resolve<my_style, my_second_style>();

    REQUIRE(style.gap == centui::length{30, centui::unit::px});
    REQUIRE(style.position == centui::position::absolute);

    REQUIRE(style.padding.top == centui::px(20));
    REQUIRE(style.padding.right == centui::px(20));
    REQUIRE(style.padding.bottom == centui::px(20));
    REQUIRE(style.padding.left == centui::px(40));
}

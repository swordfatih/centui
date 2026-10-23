#include <limits>
#include <stdexcept>

#include <centui/style.hpp>
#include <centui/types.hpp>

#include <catch2/catch_test_macros.hpp>

struct my_style
{
    centui::length gap = centui::px(20);
    centui::edges padding = centui::edges::all(centui::px(20));
};

struct my_second_style
{
    centui::length gap = centui::px(30);
    centui::position position = centui::position::absolute;

    struct
    {
        centui::length left = centui::px(40);
    } padding;
};

TEST_CASE("Resolve style from struct", "[style]")
{
    const auto style = centui::style::resolve<my_style>();

    REQUIRE(style.gap == centui::length{20, centui::unit::px});
    REQUIRE(style.padding == centui::edges::all(centui::px(20)));
}

TEST_CASE("Resolve multiple styles", "[style]")
{
    const auto style = centui::style::resolve<my_style, my_second_style>();

    REQUIRE(style.gap == centui::length{30, centui::unit::px});
    REQUIRE(style.position == centui::position::absolute);

    REQUIRE(style.padding.top == centui::px(20));
    REQUIRE(style.padding.right == centui::px(20));
    REQUIRE(style.padding.bottom == centui::px(20));
    REQUIRE(style.padding.left == centui::px(40));
}

TEST_CASE("Partial state overrides retain unspecified colors", "[style]")
{
    struct partial
    {
        struct
        {
            centui::color hover = centui::color::from_hex(0x123456);
        } background_color;
    };

    const auto appearance = centui::style::resolve<partial>();
    REQUIRE(appearance.background_color.hover == centui::color::from_hex(0x123456));
    REQUIRE(appearance.background_color.normal.a == 0);
}

TEST_CASE("Style patches validate atomically and classify changes", "[style]")
{
    const centui::style original;
    const auto painted = centui::style_patch{.text_color = centui::states_color::all({1, 2, 3, 255})}.apply(original);
    REQUIRE(painted.changes_from(original).paint);
    REQUIRE_FALSE(painted.changes_from(original).layout);
    REQUIRE_FALSE(original.changes_from(original).any());
    const auto resized = centui::style_patch{.width = centui::px(200)}.apply(original);
    REQUIRE(resized.changes_from(original).layout);
    REQUIRE_THROWS_AS((centui::style_patch{.font_size = -1}.apply(original)), std::invalid_argument);
    REQUIRE_THROWS_AS(
        (centui::style_patch{.width = centui::px(std::numeric_limits<float>::infinity())}.apply(original)),
        std::invalid_argument
    );
    REQUIRE(original.font_size == 16);
}

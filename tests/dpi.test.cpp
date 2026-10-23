#include "test_font.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

#include <centui/centui.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Display scale changes bitmap resolution without changing text metrics", "[dpi][font]")
{
    centui::text_service fonts{centui::font{test_font_path()}};
    const auto original = fonts.prepare("Hello", {}, 16);

    fonts.set_scale(2);
    const auto dense = fonts.prepare("Hello", {}, 16);
    REQUIRE(dense->bounds == original->bounds);
    REQUIRE(dense->raster_scale == 2);
    REQUIRE(dense->glyphs.front().image->height > original->glyphs.front().image->height);

    fonts.set_scale(1);
    const auto restored = fonts.prepare("Hello", {}, 16);
    REQUIRE(restored->glyphs.front().image == original->glyphs.front().image);
    REQUIRE_THROWS_AS(fonts.set_scale(0), std::invalid_argument);
    REQUIRE_THROWS_AS(fonts.set_scale(std::numeric_limits<float>::infinity()), std::invalid_argument);
}

TEST_CASE("Documents refresh text caches and hit test in logical coordinates", "[dpi][document]")
{
    int clicks = 0;
    auto action = centui::button{
        "Hello",
        {
            .on_click = [&clicks]
            {
                ++clicks;
            },
        }
    };
    action->patch_style({.width = centui::px(80), .height = centui::px(30)});
    auto root = centui::container{action};
    root->patch_style({.width = centui::percent(100), .height = centui::percent(100)});
    centui::document document{root};
    document.set_font(centui::font{test_font_path()});

    (void)document.prepare({200, 100, 1});
    const auto& dense = document.prepare({400, 200, 2});
    REQUIRE(root->bounds().width == 200);
    REQUIRE(action->bounds().width == 80);

    bool found_text = false;

    for(const auto& command: dense.commands)
    {
        if(std::holds_alternative<centui::text_command>(command))
        {
            found_text = true;
            REQUIRE(std::get<centui::text_command>(command).shaped->raster_scale == 2);
        }
    }

    REQUIRE(found_text);

    document.process_event({centui::input_event_type::pointer_pressed, 60, 15}, {400, 200, 2});
    document.process_event({centui::input_event_type::pointer_released, 60, 15}, {400, 200, 2});
    REQUIRE(clicks == 1);
    (void)document.prepare({400, 200, 1});
    REQUIRE(root->bounds().width == 400);
}

TEST_CASE("Dense glyph clipping uses logical bounds and physical pixel alignment", "[dpi][render]")
{
    auto layout = std::make_shared<centui::text_layout>();
    layout->raster_scale = 2;
    layout->ascender = 4;
    auto image = std::make_shared<const centui::glyph_bitmap>(10, 8, 0, 8, std::vector<std::uint8_t>(80, 255));
    layout->glyphs.push_back({1, 0, 0, 0, image});
    layout->lines.push_back({0, 1, 5});
    const centui::text_command
        command{{0.3f, 0, 20, 20}, {255, 255, 255, 255}, layout, centui::text_alignment::left, {1, 0, 20, 20}};
    std::vector<centui::glyph_quad> quads;
    centui::details::for_each_glyph(
        command,
        {0, 0, 100, 100},
        [&](const auto& quad)
        {
            quads.push_back(quad);
        }
    );
    REQUIRE(quads.size() == 1);
    REQUIRE(quads.front().bounds.x == 1);
    REQUIRE(quads.front().bounds.width == Catch::Approx(4.5f));
    REQUIRE(quads.front().bounds.height == 4);
    REQUIRE(quads.front().uv.x == Catch::Approx(0.1f));
}

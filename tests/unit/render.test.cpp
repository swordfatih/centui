#include "layout_data.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include <centui/render.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <centypo/engine.hpp>

TEST_CASE("Glyph clipping preserves placement and texture coordinates", "[render]")
{
    const auto image = std::make_shared<const centypo::glyph_bitmap>(
        centypo::glyph_bitmap{10, 8, -2, 6, std::vector<std::uint8_t>(80, 255)}
    );
    centypo::details::layout_data data;
    data.ascender = 6;
    data.glyphs.push_back({1, 0, 0, 0, image});
    data.lines.push_back({0, 1, 10});
    const auto layout =
        centypo::details::layout_access::make(std::make_shared<const centypo::details::layout_data>(std::move(data)));
    const centui::text_command
        command{{10, 20, 20, 10}, {10, 20, 30, 128}, layout.aligned(20, centypo::text_alignment::center), {15, 22, 10, 6}};
    std::vector<centui::glyph_quad> quads;
    centui::details::for_each_glyph(
        command,
        {0, 0, 21, 26},
        [&](auto quad)
        {
            quads.push_back(std::move(quad));
        }
    );

    REQUIRE(quads.size() == 1);
    const auto& quad = quads.front();
    REQUIRE(quad.bounds.x == 15);
    REQUIRE(quad.bounds.y == 22);
    REQUIRE(quad.bounds.width == 6);
    REQUIRE(quad.bounds.height == 4);
    REQUIRE(quad.uv.x == Catch::Approx(0.2f));
    REQUIRE(quad.uv.y == Catch::Approx(0.25f));
    REQUIRE(quad.uv.width == Catch::Approx(0.6f));
    REQUIRE(quad.uv.height == Catch::Approx(0.5f));
    REQUIRE(quad.image == image);
    REQUIRE(quad.color == command.color);
}

TEST_CASE("Clipped or transparent glyph runs produce no drawing", "[render]")
{
    centypo::details::layout_data data;
    data.ascender = 2;
    data.glyphs.push_back(
        {1,
         0,
         0,
         0,
         std::make_shared<const centypo::glyph_bitmap>(
             centypo::glyph_bitmap{2, 2, 0, 2, std::vector<std::uint8_t>(4, 255)}
         )}
    );
    data.lines.push_back({0, 1, 2});
    const auto layout =
        centypo::details::layout_access::make(std::make_shared<const centypo::details::layout_data>(std::move(data)));
    centui::text_command command{
        {0, 0, 10, 10},
        {255, 255, 255, 255},
        layout,
        {20, 20, 10, 10},
    };
    std::size_t count = 0;
    const auto draw = [&](const auto&)
    {
        ++count;
    };
    centui::details::for_each_glyph(command, {0, 0, 100, 100}, draw);
    command.clip = command.bounds;
    command.color.a = 0;
    centui::details::for_each_glyph(command, {0, 0, 100, 100}, draw);
    REQUIRE(count == 0);
}

TEST_CASE("Draw resources survive CPU cache eviction", "[font][render]")
{
    centypo::engine fonts({.system_fonts = false});
    fonts.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    const centypo::font face{{"Inter"}};
    auto prepared = fonts.rasterize(fonts.layout("A", {.size = 20, .font = face}));
    REQUIRE_FALSE(prepared.glyphs().empty());
    auto image = prepared.glyphs().front().image;
    REQUIRE(image);
    REQUIRE_FALSE(image->pixels.empty());
    const std::weak_ptr<const centypo::glyph_bitmap> retained_image = image;
    const auto pixels = image->pixels;
    auto cached = fonts.rasterize(fonts.layout("A", {.size = 20, .font = face}));
    REQUIRE(cached.glyphs().front().image == image);

    centui::draw_list output{{100, 100}, {}};
    centui::details::for_each_glyph(
        {{0, 0, 100, 100}, {255, 255, 255, 255}, prepared, {0, 0, 100, 100}},
        {0, 0, 100, 100},
        [&](const auto& quad)
        {
            output.commands.push_back({quad.bounds, quad.uv, quad.image, quad.color});
        }
    );
    prepared = {};
    fonts.set_cache_budgets({.images = 0});
    const auto uncached = fonts.rasterize(fonts.layout("A", {.size = 20, .font = face}));
    REQUIRE(uncached.glyphs().front().image != image);
    fonts.clear_cache();
    image.reset();
    cached = {};
    REQUIRE_FALSE(retained_image.expired());

    REQUIRE_FALSE(output.commands.empty());
    for(const auto& quad: output.commands)
    {
        REQUIRE(quad.image == retained_image.lock());
        REQUIRE(quad.image->pixels == pixels);
    }
}

TEST_CASE("Wrapping and CRLF retain original UTF-8 byte clusters", "[font][render]")
{
    centypo::engine fonts({.system_fonts = false});
    fonts.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    const centypo::font face{{"Inter"}};
    const auto word = fonts.layout("\xC3\xA9", {.size = 20, .font = face});
    const auto layout =
        fonts.layout("\xC3\xA9 \xC3\xA9\r\nZ", {.size = 20, .width = word.bounds().width, .font = face});
    REQUIRE(layout.lines().size() == 3);
    REQUIRE(layout.bounds().x == 0);
    REQUIRE(layout.bounds().y == 0);
    REQUIRE(layout.bounds().height == Catch::Approx(word.bounds().height * 3));
    const std::vector<std::uint32_t> starts{0, 3, 7};

    for(std::size_t i = 0; i < starts.size(); ++i)
    {
        const auto& line = layout.lines()[i];
        REQUIRE(line.begin < line.end);

        for(auto glyph = line.begin; glyph < line.end; ++glyph)
        {
            REQUIRE(layout.glyphs()[glyph].cluster >= starts[i]);
            REQUIRE(layout.glyphs()[glyph].cluster < line.text_end);
        }
    }

    const auto long_word = fonts.layout("overflow", {.size = 20, .width = 1, .font = face});
    REQUIRE(long_word.lines().size() == 1);
    REQUIRE(long_word.bounds().width > 1);
    const auto empty_lines = fonts.layout("\r\n\n", {.size = 20, .font = face});
    REQUIRE(empty_lines.lines().size() == 3);
    REQUIRE(empty_lines.glyphs().empty());
}

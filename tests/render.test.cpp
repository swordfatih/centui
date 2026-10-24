#include "test_font.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include <centui/font.hpp>
#include <centui/render.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Glyph clipping preserves placement and texture coordinates", "[render]")
{
    const auto image = std::make_shared<const centui::glyph_bitmap>(
        centui::glyph_bitmap{10, 8, -2, 6, std::vector<std::uint8_t>(80, 255)}
    );
    auto layout = std::make_shared<centui::text_layout>();
    layout->ascender = 6;
    layout->glyphs.push_back({1, 0, 0, 0, image});
    layout->lines.push_back({0, 1, 10});
    const centui::text_command
        command{{10, 20, 20, 10}, {10, 20, 30, 128}, layout, centui::text_alignment::center, {15, 22, 10, 6}};
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
    auto layout = std::make_shared<centui::text_layout>();
    layout->ascender = 2;
    layout->glyphs.push_back(
        {1,
         0,
         0,
         0,
         std::make_shared<const centui::glyph_bitmap>(
             centui::glyph_bitmap{2, 2, 0, 2, std::vector<std::uint8_t>(4, 255)}
         )}
    );
    layout->lines.push_back({0, 1, 2});
    centui::text_command command{
        {0, 0, 10, 10},
        {255, 255, 255, 255},
        layout,
        centui::text_alignment::left,
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

TEST_CASE("Draw resources survive CPU cache eviction and service destruction", "[font][render]")
{
    centui::text_service fonts;
    const centui::font face{test_font_path()};
    auto prepared = fonts.prepare("A", face, 20);
    REQUIRE_FALSE(prepared->glyphs.empty());
    auto image = prepared->glyphs.front().image;
    REQUIRE(image);
    REQUIRE_FALSE(image->pixels.empty());
    const std::weak_ptr<const centui::glyph_bitmap> retained_image = image;
    const auto pixels = image->pixels;
    auto cached = fonts.prepare("A", face, 20);
    REQUIRE(cached->glyphs.front().image == image);

    centui::draw_list output{{100, 100}, {}};
    centui::details::for_each_glyph(
        {{0, 0, 100, 100}, {255, 255, 255, 255}, prepared, centui::text_alignment::left, {0, 0, 100, 100}},
        {0, 0, 100, 100},
        [&](const auto& quad)
        {
            output.commands.push_back({quad.bounds, quad.uv, quad.image, quad.color});
        }
    );
    prepared.reset();
    fonts.set_cache_budget(0);
    const auto uncached = fonts.prepare("A", face, 20);
    REQUIRE(uncached->glyphs.front().image != image);
    fonts = centui::text_service{};
    image.reset();
    cached.reset();
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
    centui::text_service fonts;
    const centui::font face{test_font_path()};
    const auto word = fonts.prepare("\xC3\xA9", face, 20);
    const auto layout = fonts.prepare("\xC3\xA9 \xC3\xA9\r\nZ", face, 20, word->bounds.width);
    REQUIRE(layout->lines.size() == 3);
    REQUIRE(layout->bounds.x == 0);
    REQUIRE(layout->bounds.y == 0);
    REQUIRE(layout->bounds.height == Catch::Approx(word->bounds.height * 3));
    const std::vector<std::uint32_t> starts{0, 3, 7};

    for(std::size_t i = 0; i < starts.size(); ++i)
    {
        const auto& line = layout->lines[i];
        REQUIRE(line.begin < line.end);

        for(auto glyph = line.begin; glyph < line.end; ++glyph)
        {
            REQUIRE(layout->glyphs[glyph].cluster == starts[i]);
        }
    }

    const auto long_word = fonts.prepare("overflow", face, 20, 1);
    REQUIRE(long_word->lines.size() == 1);
    REQUIRE(long_word->bounds.width > 1);
    const auto empty_lines = fonts.prepare("\r\n\n", face, 20);
    REQUIRE(empty_lines->lines.size() == 3);
    REQUIRE(empty_lines->glyphs.empty());
}

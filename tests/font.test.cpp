#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "test_font.hpp"

import centui.font;
import centui.widgets;

TEST_CASE("Measure bundled font", "[font]")
{
    centui::font font{test_font_path()};

    const auto short_text = font.measure("Hello", 16);
    const auto long_text = font.measure("Hello world", 16);

    REQUIRE(short_text.width > 0);
    REQUIRE(short_text.height > 0);
    REQUIRE(long_text.width > short_text.width);
}

TEST_CASE("Measure text at different sizes", "[font]")
{
    centui::font font{test_font_path()};

    const auto small = font.measure("Hello", 12);
    const auto large = font.measure("Hello", 24);

    REQUIRE(large.width > small.width);
    REQUIRE(large.height > small.height);
}

TEST_CASE("Text layout supplies measurement and glyph positions", "[font]")
{
    centui::font face{test_font_path()};

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
    centui::font face{test_font_path()};

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
    centui::font original{test_font_path()};
    const auto expected = original.measure("Hello", 16);
    auto moved = std::move(original);
    REQUIRE(moved.measure("Hello", 16).width == expected.width);
    centui::font replacement{test_font_path()};
    replacement = std::move(moved);
    REQUIRE(replacement.measure("Hello", 16).width == expected.width);
    REQUIRE_THROWS_AS(centui::font(std::filesystem::path{"centui-missing-font-for-test.ttf"}), std::runtime_error);
}

TEST_CASE("Font copies share resources and styles retain ownership", "[font]")
{
    auto appearance = []
    {
        centui::font original{test_font_path()};
        const auto copy = original;
        REQUIRE(copy == original);
        REQUIRE(copy.identity() == original.identity());
        return centui::style{.font = copy};
    }();

    REQUIRE(appearance.font->measure("Retained", 16).width > 0);
}

TEST_CASE("Fonts inherit through containers and explicit styles override them", "[font]")
{
    const centui::font parent_font{test_font_path()};
    const centui::font override_font{test_font_path()};
    auto label = centui::text{"Inherited"};
    auto parent = centui::container{label};
    parent->patch_style({.font = parent_font});
    centui::document document{parent};

    REQUIRE(label->resolved_style().font == parent_font);
    REQUIRE_NOTHROW(document.prepare({200, 100}));

    label->patch_style({.font = override_font});
    REQUIRE(label->resolved_style().font == override_font);
    REQUIRE_NOTHROW(document.prepare({200, 100}));

    label->patch_style({.font = std::optional<centui::font>{}});
    REQUIRE(label->resolved_style().font == parent_font);
    parent->patch_style({.font = override_font});
    REQUIRE(document.pending_changes().layout);
    REQUIRE(label->resolved_style().font == override_font);
    REQUIRE_NOTHROW(document.prepare({200, 100}));
}

TEST_CASE("Only text requires a font and document font changes invalidate layout", "[font]")
{
    centui::document empty;
    REQUIRE_NOTHROW(empty.prepare({200, 100}));

    centui::document document{centui::text{"Explicit font"}};
    REQUIRE_THROWS_AS(document.prepare({200, 100}), std::logic_error);

    const centui::font face{test_font_path()};
    document.set_font(face);
    REQUIRE_NOTHROW(document.prepare({200, 100}));
    document.set_font(face);
    REQUIRE_FALSE(document.pending_changes().layout);

    document.set_font(centui::font{test_font_path()});
    REQUIRE(document.pending_changes().layout);
    REQUIRE_NOTHROW(document.prepare({200, 100}));
}

#include <type_traits>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <centypo/centypo.hpp>

static_assert(std::is_same_v<decltype(std::declval<centypo::engine>().layout("text")), centypo::layout>);
static_assert(std::is_nothrow_move_constructible_v<centypo::engine>);
static_assert(std::is_copy_constructible_v<centypo::layout>);
static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<centypo::layout>().glyphs()[0])>>);

TEST_CASE("Centypo layouts retain immutable geometry and fonts", "[centypo]")
{
    centypo::layout retained;
    std::uintptr_t face = 0;

    {
        centypo::engine typography({.system_fonts = false});
        typography.register_font(std::filesystem::path{CENTUI_FONT_PATH});

        auto text = typography.layout("Hello, world", {.size = 18, .width = 320});
        REQUIRE_FALSE(text.glyphs().empty());
        REQUIRE(text.bounds().width > 0);
        REQUIRE_FALSE(text.glyphs().front().image);

        face = text.glyphs().front().font_id();
        REQUIRE(face != 0);
        REQUIRE_FALSE(text.glyphs().front().family().empty());

        const auto copy = text;
        REQUIRE(copy.glyphs().data() == text.glyphs().data());

        const auto same = typography.layout("Hello, world", {.size = 18, .width = 320});
        REQUIRE(same.glyphs().data() == text.glyphs().data());
        REQUIRE(typography.statistics().layout_hits > 0);

        const auto dense = typography.rasterize(text, 2);
        REQUIRE(dense.bounds() == text.bounds());
        REQUIRE(dense.raster_scale() == 2);
        REQUIRE(dense.glyphs().front().image);
        REQUIRE_FALSE(dense.glyphs().front().image->pixels.empty());

        const auto aligned = text.aligned(320, centypo::text_alignment::right);
        REQUIRE(aligned.bounds() == text.bounds());
        REQUIRE(aligned.caret_bounds({0}).x > text.caret_bounds({0}).x);
        REQUIRE_FALSE(text.selection_bounds(0, 5).empty());

        for(const auto& caret: text.carets())
        {
            const auto bounds = text.caret_bounds({caret.offset, caret.affinity});
            const auto hit = text.hit_test({bounds.x, bounds.y + bounds.height / 2});
            REQUIRE(hit.offset == caret.offset);
        }

        const auto revision = typography.revision();
        REQUIRE_THROWS_AS(typography.layout(std::string("x\0y", 3)), std::invalid_argument);
        REQUIRE_THROWS_AS(typography.layout("x", {.size = -1}), std::invalid_argument);
        REQUIRE_THROWS_AS(typography.rasterize(text, 0), std::invalid_argument);
        REQUIRE(typography.revision() == revision);

        const std::byte invalid[]{std::byte{1}};
        // Native font discovery catches this base type to skip unusable fonts.
        REQUIRE_THROWS_AS(typography.register_font(invalid), std::exception);
        REQUIRE(typography.revision() == revision);

        typography.refresh_fonts();
        REQUIRE(typography.layout("Hello").glyphs().front().font_id() == face);

        typography.set_cache_budgets({0, 0, 0});
        REQUIRE(typography.statistics().layout_bytes == 0);

        retained = std::move(text);
        REQUIRE(text.glyphs().empty());

        auto moved = std::move(typography);
        REQUIRE_FALSE(moved.layout("Moved engine").glyphs().empty());
    }

    REQUIRE(retained.bounds().width > 0);
    REQUIRE(retained.glyphs().front().font_id() == face);

    centypo::engine another({.system_fonts = false});
    REQUIRE(another.rasterize(retained).glyphs().front().image);
}

TEST_CASE("Centypo editing preserves composition and undo transactions", "[centypo]")
{
    centypo::editor editor{true};
    editor.set_text("before");
    editor.select({0}, {6});
    editor.compose("draft", 3);

    REQUIRE(editor.value() == "before");
    REQUIRE(editor.display_text() == "draft");

    REQUIRE_THROWS_AS(editor.replace("\xc0\x80"), std::invalid_argument);
    REQUIRE(editor.display_text() == "draft");

    editor.replace("after");
    REQUIRE(editor.undo(false));
    REQUIRE(editor.value() == "before");
    REQUIRE(editor.undo(true));
    REQUIRE(editor.value() == "after");
}

TEST_CASE("Centypo analysis exposes grapheme byte boundaries", "[centypo]")
{
    REQUIRE(centypo::analyze_text("a\xcc\x81\r\nb").graphemes == std::vector<std::size_t>{0, 3, 5, 6});
}

TEST_CASE("Font revisions advance only when a collection is published", "[centypo]")
{
    centypo::engine typography({.system_fonts = false});
    auto revision = typography.revision();
    typography.clear_cache();
    typography.set_cache_budgets({});
    typography.set_system_font_discovery(false);
    REQUIRE(typography.revision() == revision);

    for(int i = 0; i < 4; ++i)
    {
        typography.refresh_fonts();
        REQUIRE(typography.revision() == ++revision);
    }
    typography.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    REQUIRE(typography.revision() == ++revision);
    typography.set_font_alias("sans-serif", {"Inter"});
    REQUIRE(typography.revision() == ++revision);
    typography.set_font_alias("sans-serif", {"Inter"});
    REQUIRE(typography.revision() == revision);
    const auto moved = std::move(typography);
    REQUIRE(moved.revision() == revision);
}

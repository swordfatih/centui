#include <centui/document.hpp>
#include <centui/widgets/text.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Reflow reuses shaping and raster scale does not change logical geometry", "[font][cache]")
{
    centui::document service_document{centui::document_options{.system_font_discovery = false}};
    service_document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto& service = service_document.typography();
    const auto options = centypo::layout_options{.size = 20, .width = 100};
    const auto logical = service.layout("one two three four", options);
    const auto first = service.rasterize(logical);
    const auto initial = service.statistics();
    const auto again = service.rasterize(service.layout("one two three four", options));
    REQUIRE(service.statistics().layout_hits > initial.layout_hits);
    REQUIRE(again.glyphs().data() == first.glyphs().data());
    REQUIRE(again.glyphs().front().image == first.glyphs().front().image);
    (void)service.layout("one two three four", {.size = 20, .width = 75});
    REQUIRE(service.statistics().shaping_hits > initial.shaping_hits);
    const auto dense = service.rasterize(logical, 2);
    REQUIRE(dense.bounds() == first.bounds());
    REQUIRE(dense.glyphs().front().image != first.glyphs().front().image);
}

TEST_CASE("Zero budgets disable admission and retained layouts survive eviction", "[font][cache]")
{
    centui::document service_document{centui::document_options{.system_font_discovery = false}};
    service_document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto& service = service_document.typography();
    auto retained = service.rasterize(service.layout("retained", {.size = 20, .width = 100}));
    const auto image = retained.glyphs().front().image;
    service_document.set_text_cache_budgets({0, 0, 0});
    const auto fresh = service.rasterize(service.layout("retained", {.size = 20, .width = 100}));
    const auto stats = service.statistics();
    REQUIRE(stats.shaping_bytes == 0);
    REQUIRE(stats.layout_bytes == 0);
    REQUIRE(stats.image_bytes == 0);
    REQUIRE(stats.retained_bytes > 0);
    REQUIRE(stats.font_bytes > 0);
    REQUIRE(fresh.glyphs().front().image != image);
    REQUIRE_FALSE(image->pixels.empty());
    REQUIRE(retained.glyphs().front().image == image);
}

TEST_CASE("Centypo prepared layouts survive rejected requests and cache eviction", "[font][cache]")
{
    centypo::engine engine({.system_fonts = false});
    engine.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    const auto valid = engine.layout("retained", {.size = 20});
    const auto raster = engine.rasterize(valid);

    REQUIRE_THROWS_AS(engine.layout("replacement", {.size = -1}), std::invalid_argument);
    REQUIRE(engine.rasterize(valid).glyphs().data() == raster.glyphs().data());
    REQUIRE(engine.layout("retained", {.size = 20}).glyphs().data() == valid.glyphs().data());

    REQUIRE_THROWS_AS(engine.rasterize(valid, 0), std::invalid_argument);
    REQUIRE(engine.rasterize(valid).glyphs().data() == raster.glyphs().data());

    const auto pixels = raster.glyphs().front().image->pixels;
    engine.clear_cache();
    REQUIRE(engine.rasterize(raster).glyphs().data() == raster.glyphs().data());
    REQUIRE(raster.glyphs().front().image->pixels == pixels);
    REQUIRE(engine.rasterize(valid).glyphs().data() != raster.glyphs().data());
    REQUIRE(engine.rasterize(centypo::layout{}, 2).raster_scale() == 2);
}

TEST_CASE("Centypo reuses aligned layouts and keeps raster scales distinct", "[font][cache]")
{
    centypo::engine engine({.system_fonts = false});
    engine.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    const auto options = centypo::layout_options{.size = 20, .width = 300, .alignment = centypo::text_alignment::right};

    const auto aligned = engine.layout("aligned", options);
    REQUIRE(engine.layout("aligned", options).glyphs().data() == aligned.glyphs().data());
    REQUIRE(aligned.caret_bounds({0}).x > 0);

    const auto normal = engine.rasterize(aligned);
    const auto dense = engine.rasterize(aligned, 2);
    REQUIRE(engine.rasterize(aligned).glyphs().data() == normal.glyphs().data());
    REQUIRE(engine.rasterize(aligned, 2).glyphs().data() == dense.glyphs().data());
    REQUIRE(engine.rasterize(dense, 2).glyphs().data() == dense.glyphs().data());
    REQUIRE(normal.glyphs().data() != dense.glyphs().data());
    REQUIRE(normal.bounds() == dense.bounds());
    REQUIRE(normal.caret_bounds({0}) == dense.caret_bounds({0}));
    REQUIRE_FALSE(aligned.glyphs().front().image);
}

TEST_CASE("Prepared layout admission respects layout and image budgets", "[font][cache]")
{
    centypo::text_cache_budgets budgets;
    SECTION("layout admission disabled")
    {
        budgets.layouts = 0;
    }
    SECTION("image admission disabled")
    {
        budgets.images = 0;
    }
    SECTION("retained pixels exceed the layout budget")
    {
        budgets.layouts = 1024;
    }

    centypo::engine engine({.system_fonts = false, .caches = budgets});
    engine.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    const auto logical = engine.layout("A", {.size = 100});
    const auto before = engine.statistics().layout_bytes;

    const auto first = engine.rasterize(logical);
    const auto second = engine.rasterize(logical);
    REQUIRE(first.glyphs().data() != second.glyphs().data());
    REQUIRE(engine.rasterize(first).glyphs().data() == first.glyphs().data());
    REQUIRE(engine.statistics().layout_bytes == before);
    REQUIRE(engine.statistics().layout_bytes <= budgets.layouts);
    REQUIRE(engine.statistics().image_bytes <= budgets.images);
}

TEST_CASE("Prepared drawing resources survive their document", "[font][lifetime][render]")
{
    centui::draw_list retained;
    {
        centui::document document{centui::text{"Retained drawing"}, {.system_font_discovery = false}};
        document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
        retained = document.prepare({300, 100});
    }
    REQUIRE_FALSE(retained.commands.empty());
    REQUIRE(
        std::ranges::any_of(
            retained.commands,
            [](const auto& command)
            {
                return command.image && !command.image->pixels.empty();
            }
        )
    );
}

TEST_CASE("Wrapping bounds shaping work for long paragraphs", "[font][cache][wrapping]")
{
    centypo::engine engine({.system_fonts = false});
    engine.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    std::string text;
    for(int i = 0; i < 256; ++i)
    {
        text += "word ";
    }

    float width = 100;
    SECTION("many short lines")
    {
        width = 100;
    }
    SECTION("one wide line")
    {
        width = 100000;
    }

    const auto before = engine.statistics().shapes;
    const auto layout = engine.layout(text, {.size = 16, .width = width});
    REQUIRE(engine.statistics().shapes - before < 32);
    REQUIRE(layout.lines().front().text_begin == 0);
    REQUIRE(layout.lines().back().text_end == text.size());
    REQUIRE(layout.lines().size() == (width == 100 ? 128 : 1));
    for(const auto& line: layout.lines())
    {
        REQUIRE(line.width <= width);
    }
}

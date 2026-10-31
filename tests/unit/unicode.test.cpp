#include <fstream>
#include <sstream>
#include <string>

#include <centui/document.hpp>
#include <centui/widgets/input.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{

void append_utf8(std::string& text, std::uint32_t value)
{
    if(value < 0x80)
    {
        text += static_cast<char>(value);
    }
    else if(value < 0x800)
    {
        text += static_cast<char>(0xc0 | (value >> 6));
        text += static_cast<char>(0x80 | (value & 0x3f));
    }
    else if(value < 0x10000)
    {
        text += static_cast<char>(0xe0 | (value >> 12));
        text += static_cast<char>(0x80 | ((value >> 6) & 0x3f));
        text += static_cast<char>(0x80 | (value & 0x3f));
    }
    else
    {
        text += static_cast<char>(0xf0 | (value >> 18));
        text += static_cast<char>(0x80 | ((value >> 12) & 0x3f));
        text += static_cast<char>(0x80 | ((value >> 6) & 0x3f));
        text += static_cast<char>(0x80 | (value & 0x3f));
    }
}

void check_conformance(const char* path, bool words)
{
    std::ifstream source(path);
    REQUIRE(source.is_open());
    std::string line;
    std::size_t number = 0, checked = 0;
    while(std::getline(source, line))
    {
        ++number;
        line.erase(line.find('#') == std::string::npos ? line.size() : line.find('#'));
        std::istringstream tokens(line);
        std::string token, text;
        std::vector<std::size_t> expected;
        bool contains_nul = false;
        while(tokens >> token)
        {
            if(token == "\xc3\xb7")
            {
                expected.push_back(text.size());
            }
            else if(token != "\xc3\x97")
            {
                const auto value = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
                contains_nul |= value == 0;
                append_utf8(text, value);
            }
        }
        // NUL is deliberately outside CentUI's text contract.
        if(expected.empty() || contains_nul)
        {
            continue;
        }
        INFO("Unicode fixture line " << number);
        const auto boundaries = centypo::analyze_text(text);
        REQUIRE((words ? boundaries.words : boundaries.graphemes) == expected);
        ++checked;
    }
    REQUIRE(checked > 100);
}

} // namespace

TEST_CASE("Unicode 17 grapheme and word conformance", "[text][unicode][conformance]")
{
    SECTION("graphemes")
    {
        check_conformance(CENTUI_GRAPHEME_TEST_PATH, false);
    }
    SECTION("words")
    {
        check_conformance(CENTUI_WORD_TEST_PATH, true);
    }
}

TEST_CASE("Text validation is literal and host offsets preserve scalars", "[text][unicode]")
{
    for(const auto& invalid:
        {std::string("a\0b", 3),
         std::string("\xc0\x80"),
         std::string("\xed\xa0\x80"),
         std::string("\xf4\x90\x80\x80"),
         std::string("\xe2\x82"),
         std::string("\x80")})
    {
        REQUIRE_THROWS_AS(centypo::validate_text(invalid), std::invalid_argument);
    }
    REQUIRE_NOTHROW(centypo::validate_text("<b>literal</b>\t\r\n"));
    const std::string value = "a\xf0\x9f\x98\x80z";
    REQUIRE(centypo::text_byte_offset(value, 2) == 1);
    REQUIRE(centypo::text_byte_offset(value, 3) == 5);
    REQUIRE(centypo::text_utf16_offset(value, 4) == 1);
    REQUIRE(centypo::text_utf16_offset(value, 5) == 3);
    REQUIRE(centypo::analyze_text("a\xcc\x81\r\nb").graphemes == std::vector<std::size_t>{0, 3, 5, 6});
}

TEST_CASE("Paragraph modes preserve spaces, tabs, breaks and logical scale", "[text][layout]")
{
    centui::document service_document{centui::document_options{.system_font_discovery = false}};
    service_document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto& service = service_document.typography();
    auto options = centypo::text_options{.wrap = centypo::text_wrap::none, .tab_stops = {80}, .line_spacing = 3};
    const auto layout = service.layout("A\tB\r\n\nZ", {.size = 19.5f, .width = 30, .paragraph = options});
    REQUIRE(layout.lines().size() == 3);
    REQUIRE(layout.lines()[0].text_end == 3);
    REQUIRE(layout.lines()[1].text_begin == 5);
    REQUIRE(layout.lines()[2].text_begin == 6);
    REQUIRE(layout.caret_bounds({2}).x == Catch::Approx(80));
    REQUIRE(layout.lines()[1].y == Catch::Approx(layout.lines()[0].height + 3));
    REQUIRE(service.rasterize(layout, 2).bounds() == layout.bounds());
    REQUIRE(service.layout("overflow", {.size = 16, .width = 1}).lines().size() == 1);
    options.wrap = centypo::text_wrap::word_grapheme;
    REQUIRE(service.layout("overflow", {.size = 16, .width = 1, .paragraph = options}).lines().size() >= 6);
    options.tab_stops = {10, 10};
    REQUIRE_THROWS_AS(service.layout("x", {.size = 16, .width = 100, .paragraph = options}), std::invalid_argument);
}

TEST_CASE("Ligature carets include every grapheme and invalid edits roll back", "[text][input]")
{
    centui::document service_document{centui::document_options{.system_font_discovery = false}};
    service_document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto& service = service_document.typography();
    const auto layout = service.layout("office", {.size = 20});
    for(std::size_t offset = 0; offset <= 6; ++offset)
    {
        REQUIRE(
            std::ranges::any_of(
                layout.carets(),
                [&](const auto& caret)
                {
                    return caret.offset == offset;
                }
            )
        );
    }
    auto input = centui::input{"A\tB\r\nC", {.multiline = true}};
    REQUIRE(input->value() == "A\tB\r\nC");
    input->set_selection(1, 3);
    REQUIRE_THROWS_AS(input->set_text(std::string("x\0y", 3)), std::invalid_argument);
    REQUIRE(input->value() == "A\tB\r\nC");
    REQUIRE(input->selection() == std::pair<std::size_t, std::size_t>{1, 3});
    auto single = centui::input{"A\tB\r\nC\rD"};
    REQUIRE(single->value() == "A\tB C D");
}

TEST_CASE("Alignment geometry and whole-text truncation share the displayed layout", "[text][layout]")
{
    centui::document service_document{centui::document_options{.system_font_discovery = false}};
    service_document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto& service = service_document.typography();
    auto options = centypo::text_options{.wrap = centypo::text_wrap::none};
    const auto left = service.layout("office", {.size = 20, .width = 300, .paragraph = options});
    const auto raster = service.rasterize(left);
    const auto right = service.layout(
        "office",
        {.size = 20, .width = 300, .paragraph = options, .alignment = centypo::text_alignment::right}
    );
    REQUIRE(right.bounds() == left.bounds());
    REQUIRE(right.caret_bounds({0}).x == Catch::Approx(300 - left.bounds().width));
    REQUIRE(service.rasterize(right).glyphs().front().image == raster.glyphs().front().image);
    options.max_lines = 1;
    options.ellipsis = true;
    const auto limited = service.layout("first\r\nsecond\nthird", {.size = 20, .width = 80, .paragraph = options});
    REQUIRE(limited.lines().size() == 1);
    for(const auto& caret: limited.carets())
    {
        REQUIRE(caret.offset <= limited.lines().back().text_end);
    }
    for(const auto& cell: limited.selection_cells())
    {
        REQUIRE(cell.end <= limited.lines().back().text_end);
    }
    REQUIRE(limited.selection_bounds(7, 19).empty());
}

TEST_CASE("Unsupported joined sequences retain one normal missing-glyph cell", "[text][fallback]")
{
    centui::document service_document{centui::document_options{.system_font_discovery = false}};
    service_document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto& service = service_document.typography();
    const auto layout = service.layout("x\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x9a\x80y", {.size = 20});
    REQUIRE(
        std::count_if(
            layout.glyphs().begin(),
            layout.glyphs().end(),
            [](const auto& glyph)
            {
                return glyph.id == 0;
            }
        ) == 1
    );
    REQUIRE(layout.caret_bounds({12}).x > layout.caret_bounds({1}).x);
    const auto selected = layout.selection_bounds(1, 12);
    REQUIRE(selected.size() == 1);
    REQUIRE(selected.front().width > 0);
}

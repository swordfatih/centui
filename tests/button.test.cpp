#include "test_font.hpp"

#include <string>
#include <variant>
#include <vector>

#include <centui/centui.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{

struct label_paint
{
    std::vector<centui::draw_quad> glyphs;
    centui::color color;
};

label_paint label_command(centui::document& document)
{
    label_paint result;
    for(const auto& command: document.prepare({400, 200}).commands)
    {
        if(command.image)
        {
            result.glyphs.push_back(command);
        }
    }
    REQUIRE_FALSE(result.glyphs.empty());
    result.color = result.glyphs.front().color;
    return result;
}

} // namespace

TEST_CASE("Button labels retain their child and invalidate layout when edited", "[widget][button]")
{
    auto button = centui::button{"I"};
    auto root = centui::container{button};
    root.patch_style({.align_items = centui::alignment::start});
    centui::document document{root};
    document.set_font(centui::font{test_font_path()});
    const auto children = button->children();
    REQUIRE(children.size() == 1);
    REQUIRE(children.front()->parent() == centui::handle<>{button});
    const auto first = label_command(document);
    const auto width = button->bounds().width;

    button.set_text("A much longer label");
    REQUIRE(document.pending_changes().layout);
    const auto updated = label_command(document);
    REQUIRE(button.value() == "A much longer label");
    REQUIRE(button->children().front() == children.front());
    REQUIRE(button->bounds().width > width);
    REQUIRE(updated.glyphs.size() > first.glyphs.size());

    button->set_text("A much longer label");
    REQUIRE_FALSE(document.pending_changes().layout);
    REQUIRE_FALSE(document.pending_changes().paint);
    button.value("Done");
    REQUIRE(button->value() == "Done");
}

TEST_CASE("Button labels follow parent typography and padding changes", "[widget][button][style]")
{
    auto button = centui::button{"Label"};
    button.patch_style({
        .padding = centui::edges::all(centui::px(8)),
        .width = centui::px(180),
        .height = centui::px(80),
        .font_size = 12,
        .text_align = centui::text_alignment::right,
    });
    centui::document document{button};
    document.set_font(centui::font{test_font_path()});
    const auto small = label_command(document);
    centui::text_service fonts{centui::font{test_font_path()}};
    const auto small_layout = fonts.prepare("Label", {}, 12);
    const auto small_right = small.glyphs.back().bounds.x + small.glyphs.back().bounds.width;
    const auto content = button->content_bounds();
    REQUIRE(small.glyphs.front().bounds.x >= content.x + content.width - small_layout->bounds.width - 1);
    REQUIRE(small_right <= content.x + content.width + 1);

    button.patch_style({.font_size = 24, .text_align = centui::text_alignment::center});
    const auto large = label_command(document);
    const auto large_left = large.glyphs.front().bounds.x;
    const auto large_right = large.glyphs.back().bounds.x + large.glyphs.back().bounds.width;
    REQUIRE((large_left + large_right) / 2 == Catch::Approx(content.x + content.width / 2).margin(2));
    REQUIRE(large.glyphs.front().image->height > small.glyphs.front().image->height);
    REQUIRE(large_right - large_left > small_right - small.glyphs.front().bounds.x);
}

TEST_CASE("Button labels paint normal hover pressed focused and disabled colors", "[widget][button][style]")
{
    auto colors = centui::states_color::all({10, 0, 0, 255});
    colors.hover = {20, 0, 0, 255};
    colors.pressed = {30, 0, 0, 255};
    colors.focused = {40, 0, 0, 255};
    colors.disabled = {50, 0, 0, 255};
    auto button = centui::button{"Label"};
    button.patch_style({.width = centui::px(120), .height = centui::px(40), .text_color = colors});
    centui::document document{button};
    document.set_font(centui::font{test_font_path()});
    REQUIRE(label_command(document).color == colors.normal);
    document.process_event({centui::input_event_type::pointer_moved, 5, 5}, {400, 200});
    REQUIRE(label_command(document).color == colors.hover);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {400, 200});
    REQUIRE(label_command(document).color == colors.pressed);
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {400, 200});
    document.process_event({centui::input_event_type::pointer_left}, {400, 200});
    REQUIRE(label_command(document).color == colors.focused);
    button.set_enabled(false);
    REQUIRE(label_command(document).color == colors.disabled);
}

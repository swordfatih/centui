#include "test_font.hpp"

#include <string>
#include <variant>
#include <vector>

#include <centui/widgets/button.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

centui::text_command label_command(centui::document& document)
{
    std::vector<centui::text_command> labels;

    for(const auto& command: document.prepare({400, 200}).commands)
    {
        if(std::holds_alternative<centui::text_command>(command))
        {
            labels.push_back(std::get<centui::text_command>(command));
        }
    }

    REQUIRE(labels.size() == 1);

    return labels.front();
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
    REQUIRE(updated.shaped->glyphs.size() > first.shaped->glyphs.size());

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
    REQUIRE(small.alignment == centui::text_alignment::right);
    REQUIRE(small.bounds.x == button->content_bounds().x);
    REQUIRE(small.bounds.y == button->content_bounds().y);
    REQUIRE(small.bounds.width == button->content_bounds().width);

    button.patch_style({.font_size = 24, .text_align = centui::text_alignment::center});
    const auto large = label_command(document);
    REQUIRE(large.alignment == centui::text_alignment::center);
    REQUIRE(large.shaped->line_height > small.shaped->line_height);
    REQUIRE(large.shaped->bounds.width > small.shaped->bounds.width);
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

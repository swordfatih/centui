#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <typeinfo>
#include <variant>
#include <vector>

import centui.app;
import centui.document;
import centui.container;
import centui.text;
import centui.button;
import centui.color;
import centui.unit;

struct panel_style
{
    centui::length       width = centui::px(400);
    centui::length       height = centui::px(300);
    centui::edges        padding = centui::edges::all(centui::px(20));
    centui::states_color background_color = centui::states_color::all(centui::color::from_hex(0x334155));
};

struct child_style
{
    centui::length       width = centui::fit();
    centui::length       height = centui::px(100);
    centui::states_color background_color = centui::states_color::all(centui::color::from_hex(0x38BDF8));
};

struct title_style
{
    float                font_size = 32.0f;
    centui::states_color text_color = centui::states_color::all(centui::color::from_hex(0xFFFFFF));
};

struct action_button_style
{
    centui::length       width = centui::px(180);
    centui::length       height = centui::px(48);
    centui::states_color text_color = centui::states_color::all(centui::color::from_hex(0xFFFFFF));
    struct
    {
        centui::color normal = centui::color::from_hex(0x2563EB);
        centui::color hover = centui::color::from_hex(0x1D4ED8);
        centui::color pressed = centui::color::from_hex(0x1E40AF);
        centui::color focused = centui::color::from_hex(0x1D4ED8);
        centui::color disabled = centui::color::from_hex(0x64748B);
    } background_color;
};

int main()
{
    centui::app app{{.title = "CentUI", .width = 800, .height = 600}};

    // clang-format off
    auto title = centui::text<title_style>{"Hello, CentUI!"};
    auto action = centui::button<action_button_style>{
        "Click me",
        {
            .on_click = [title]() mutable
            {
                title.set_text("Button clicked!");
                title.patch_style({.text_color = centui::states_color::all(centui::color::from_hex(0x38BDF8))});
            },
        },
    };

    auto root = centui::container<panel_style>{
        title,
        centui::container<child_style>{
            centui::text<title_style>{"This is a nested text..."},
        },
        action,
    };
    // clang-format on

    centui::document document{root};
    app.run(document);
}

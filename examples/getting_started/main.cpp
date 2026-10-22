#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <string>

import centui;
import centui.sdl;
import centui.bgfx;

namespace
{
struct screen
{
    centui::length        width = centui::percent(100);
    centui::length        height = centui::percent(100);
    centui::alignment     align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
    centui::states_color  background_color = centui::states_color::all({15, 23, 42, 255});
};

struct card
{
    centui::length       width = centui::px(380);
    centui::edges        padding = centui::edges::all(centui::px(28));
    centui::length       gap = centui::px(18);
    centui::states_color background_color = centui::states_color::all({30, 41, 59, 255});
};

struct title
{
    float font_size = 30;
};

struct caption
{
    centui::states_color text_color = centui::states_color::all({148, 163, 184, 255});
};

struct counter
{
    float                  font_size = 56;
    centui::text_alignment text_align = centui::text_alignment::center;
    centui::states_color   text_color = centui::states_color::all({94, 234, 212, 255});
};

struct action
{
    centui::length         height = centui::px(46);
    centui::edges          padding = centui::edges::all(centui::px(12));
    centui::text_alignment text_align = centui::text_alignment::center;
    struct
    {
        centui::color normal{37, 99, 235, 255};
        centui::color hover{59, 130, 246, 255};
        centui::color pressed{29, 78, 216, 255};
        centui::color focused{59, 130, 246, 255};
    } background_color;
};
} // namespace

int main()
{
    centui::sdl::platform platform{{.title = "CentUI / Getting started", .width = 720, .height = 520}};
    centui::bgfx::backend renderer{platform};

    auto count = std::make_shared<int>(0);
    auto label = centui::text<counter>{"0"};

    // clang-format off
    auto root = centui::container<screen>{
        centui::container<card>{
            centui::text<caption>{"YOUR FIRST CENTUI APP"},
            centui::text<title>{"Hello, world."},
            centui::text<caption>{"A little state. A live label. One button."},
            label,
            centui::button<action>{"Click me", {
                .on_click = [count, label]() mutable
                {
                    label.set_text(std::format("{}", ++*count));
                },
            }},
            centui::button<action>{"Start over", {
                .on_click = [count, label]() mutable
                {
                    *count = 0;
                    label.set_text("0");
                },
            }},
        },
    };
    // clang-format on

    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::run(platform, renderer, document);
}

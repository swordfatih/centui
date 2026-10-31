#include <centui/backends/bgfx.hpp>
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>

int main()
{
    centui::sdl::platform platform{{.title = "CentUI typography", .width = 760, .height = 600}};
    centui::bgfx::backend renderer{platform};
    auto serif = centui::text{"System serif: office, affine, and combining marks."};
    serif->patch_style({.font_size = 24, .font = centypo::font{{"serif"}}});
    auto tabs = centui::text{"Name\tValue\r\nWidth\t760\r\nHeight\t600"};
    tabs->patch_style({.font = centypo::font{{"monospace"}}, .paragraph = centypo::text_options{.tab_stops = {180}}});
    auto limited = centui::text{
        "A label with a whole-text line limit. Resize the window to see truncation and ellipsis change together."
    };
    limited->patch_style({.paragraph = centypo::text_options{.max_lines = 2, .ellipsis = true}});
    auto input =
        centui::input{"Edit text, preserve tabs\tand CRLF.\r\nUndo retains selection direction.", {.multiline = true}};
    input->patch_style(
        {.height = centui::px(170), .paragraph = centypo::text_options{.wrap = centypo::text_wrap::word_grapheme}}
    );
    auto body = centui::container{serif, tabs, limited, input};
    body->patch_style(
        {.padding = centui::edges::all(centui::px(24)), .gap = centui::px(20), .width = centui::percent(100)}
    );
    auto root = centui::scroll{body};
    root->patch_style({.width = centui::percent(100), .height = centui::percent(100)});
    centui::document document{root};
    centui::ui ui{platform, renderer};
    while(platform.poll_events())
    {
        ui.frame(document);
    }
}

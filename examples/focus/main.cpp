#include <string>

#include <centui/backends/bgfx.hpp>
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>

namespace
{

struct action
{
    centui::length height = centui::px(44);
    centui::flex flex = centui::flex::all(0, 0, centui::automatic());
    centui::edges padding = centui::edges::all(centui::px(10));
    centui::states_color background_color{
        .normal = {40, 55, 80, 255},
        .hover = {65, 85, 115, 255},
        .pressed = {25, 40, 65, 255},
    };

    struct
    {
        centui::color color{100, 210, 255, 255};
        float width = 3;
    } focus;
};

struct panel
{
    centui::length width = centui::px(320);
    centui::edges padding = centui::edges::all(centui::px(20));
    centui::length gap = centui::px(12);
    centui::states_color background_color = centui::states_color::all({20, 30, 45, 255});
};

struct overlay
{
    centui::alignment align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
    centui::states_color background_color = centui::states_color::all({0, 0, 0, 160});
};

} // namespace

int main()
{
    centui::sdl::platform platform{{.title = "CentUI / Focus", .width = 640, .height = 480}};
    centui::bgfx::backend renderer{platform};
    auto close_inner = centui::button<action>{"Close nested dialog"};
    auto inner = centui::popup<overlay>{
        centui::container<panel>{centui::text{"Escape restores focus to the parent dialog."}, close_inner}
    };
    close_inner->set_on_click(
        [weak = inner.weak()]
        {
            weak.lock()->close();
        }
    );
    auto open_inner = centui::button<action>{
        "Open nested dialog",
        {.on_click = [weak = inner.weak()]
         {
             weak.lock()->open();
         }}
    };
    auto close_outer = centui::button<action>{"Close dialog"};
    auto outer = centui::popup<overlay>{centui::container<panel>{open_inner, close_outer}};
    outer->append(inner); // Nested modal ownership follows the widget tree.
    close_outer->set_on_click(
        [weak = outer.weak()]
        {
            weak.lock()->close();
        }
    );

    auto rows = centui::container{};
    rows->patch_style({.gap = centui::px(10)});
    rows->append(centui::text{"Tab shows focus, including offscreen rows. Click hides the ring; hover preserves it."});
    for(int i = 1; i <= 20; ++i)
    {
        rows->append(
            centui::button<action>{
                "Open dialog from row " + std::to_string(i),
                {.on_click = [weak = outer.weak()]
                 {
                     weak.lock()->open();
                 }}
            }
        );
    }
    auto scroller = centui::scroll{{rows}};
    scroller->patch_style({.width = centui::percent(100), .height = centui::percent(100)});
    auto root = centui::container{scroller, outer};
    root->patch_style({.width = centui::percent(100), .height = centui::percent(100)});
    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::ui ui{platform, renderer};
    while(platform.poll_events())
    {
        ui.frame(document);
    }
}

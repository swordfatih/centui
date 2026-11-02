#include <string>

#include <centui/backends/bgfx.hpp>
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>

struct screen
{
    centui::length width = centui::percent(100);
    centui::length height = centui::percent(100);
    centui::length gap = centui::px(20);
    centui::alignment align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
    float font_size = 24;
    centui::states_color text_color = centui::states_color::all({226, 232, 240, 255});
    centui::states_color background_color = centui::states_color::all({15, 23, 42, 255});
};

struct action
{
    centui::edges padding = centui::edges::all(centui::px(12));
    float corner_radius = 8;
    centui::states_color background_color{
        .normal = {37, 99, 235, 255},
        .hover = {59, 130, 246, 255},
        .pressed = {29, 78, 216, 255},
    };
};

int main()
{
    centui::sdl::platform window{{.title = "CentUI / Getting started", .width = 640, .height = 400}};
    centui::bgfx::backend renderer{window};

    int count = 0;
    auto label = centui::text{"Clicked 0 times"};

    auto root = centui::container<screen>{
        centui::text{"Hello, CentUI."},
        label,
        centui::button<action>{
            "Click me",
            {
                .on_click = [&count, label]
                {
                    label->set_text("Clicked " + std::to_string(++count) + " times");
                },
            }
        },
    };

    centui::document document{root, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    document.set_style({.font = centypo::font{"Inter"}});

    centui::ui ui{window, renderer};

    while(window.poll_events())
    {
        ui.frame(document);
    }
}

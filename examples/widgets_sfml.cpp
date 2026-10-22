#include <chrono>
#include <cstddef>
#include <format>
#include <memory>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

import centui;
import centui.sfml;
import centui.animation;

struct panel_style
{
    centui::length width = centui::percent(100);
    centui::length height = centui::percent(100);
    centui::edges padding = centui::edges::all(centui::px(24));
    centui::length gap = centui::px(12);
    centui::states_color background_color = centui::states_color::all(centui::color::from_hex(0x0F172A));
};

struct control_style
{
    centui::length height = centui::px(44);
    centui::edges padding = centui::edges::all(centui::px(10));
    centui::states_color background_color = centui::states_color::all(centui::color::from_hex(0x334155));
    float border_width = 1;
    centui::states_color border_color = centui::states_color::all(centui::color::from_hex(0x64748B));
};

struct editor_style
{
    centui::length height = centui::px(96);
    float          font_size = 18.0f;
};

struct scroll_style
{
    centui::length height = centui::px(120);
};

struct dialog_style
{
    centui::length width = centui::px(360);
    centui::length height = centui::px(160);
    centui::length gap = centui::px(16);
};

struct overlay_style
{
    centui::alignment align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
    centui::states_color background_color = centui::states_color::all({0, 0, 0, 160});
};

struct accent_style
{
    centui::length height = centui::px(6);
    centui::edges padding = centui::edges::all(centui::px(0));
    float border_width = 0;
    centui::states_color background_color = centui::states_color::all(centui::color::from_hex(0x38BDF8));
};

int main()
{
    centui::sfml::platform platform{{.title = "CentUI widgets", .width = 800, .height = 720}};
    centui::sfml::backend renderer{platform};
    centui::ui ui{platform, renderer};

    auto status = centui::text{"Tab to focus. Use arrows, Enter, and the mouse wheel."};
    auto accent = centui::container<control_style, accent_style>{};
    auto dismiss = centui::button<control_style>{"Close"};
    auto dialog = centui::popup<overlay_style>{
        centui::container<control_style, dialog_style>{
            centui::text{"A popup composed from ordinary widgets."},
            dismiss,
        },
    };
    dismiss->set_on_click([weak = dialog.weak()]
    {
        if(auto owner = weak.lock(); owner.valid())
        {
            owner->close();
        }
    });

    auto root = centui::container<panel_style>{
        centui::text{"Widget playground"},
        status,
        centui::toggle<control_style>{"Enable notifications", {
            .on_change = [status](bool checked)
            {
                status->set_text(checked ? "Notifications enabled" : "Notifications disabled");
            },
        }},
        centui::range<control_style>{{
            .minimum = 0,
            .maximum = 100,
            .step = 1,
            .value = 50,
            .on_change = [status](float value)
            {
                status->set_text(std::format("Volume: {:.0f}%", value));
            },
        }},
        centui::input<control_style>{"Edit this name", {
            .on_change = [status](std::string_view value)
            {
                status->set_text(std::format("Name: {}", value));
            },
        }},
        centui::input<control_style, editor_style>{
            "Multiline notes\nTry selection, copy/paste, and undo.", {.multiline = true},
        },
        centui::scroll<control_style, scroll_style>{
            centui::selection{
                {"Option 1", "Option 2", "Option 3", "Option 4", "Option 5", "Option 6",
                 "Option 7", "Option 8", "Option 9", "Option 10", "Option 11", "Option 12"},
                {
                    .selected = 0,
                    .on_change = [status](std::size_t index)
                    {
                        status->set_text(std::format("Selected option {}", index + 1));
                    },
                },
            },
        },
        centui::button<control_style>{"Open popup", {
            .on_click = [dialog]
            {
                dialog->open();
            },
        }},
        accent,
        dialog,
    };

    centui::document document{root};
    centui::animation<float> reveal{0, 100, std::chrono::duration<float>{1.2f}, centui::easing::curve::out_cubic};
    auto previous = std::chrono::steady_clock::now();
    while(platform.poll_events())
    {
        ui.process_events(document, platform.events());
        const auto now = std::chrono::steady_clock::now();
        if(!reveal.finished())
        {
            reveal.advance(std::chrono::duration<float>{now - previous});
            accent->patch_style({.width = centui::percent(reveal.value())});
        }
        previous = now;

        if(renderer.begin_frame())
        {
            ui.render(document);
            renderer.end_frame();
        }
    }
}

#include <chrono>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

import centui;
import centui.sdl;
import centui.bgfx;
import centui.animation;

int main()
{
    centui::sdl::platform platform{{.title = "CentUI widgets", .width = 800, .height = 720}};
    centui::bgfx::backend renderer{platform};
    centui::ui ui{platform, renderer};

    centui::style panel;
    panel.width = centui::percent(100);
    panel.height = centui::percent(100);
    panel.padding = centui::edges::all(centui::px(24));
    panel.gap = centui::px(12);
    panel.background_color = centui::states_color::all(centui::color::from_hex(0x0F172A));

    centui::style control;
    control.height = centui::px(44);
    control.padding = centui::edges::all(centui::px(10));
    control.background_color = centui::states_color::all(centui::color::from_hex(0x334155));
    control.border_width = 1;
    control.border_color = centui::states_color::all(centui::color::from_hex(0x64748B));

    auto root = centui::make_widget<centui::container_widget>(panel);
    auto title = centui::make_widget<centui::text_widget>("Widget playground");
    auto status = centui::make_widget<centui::text_widget>("Tab to focus. Use arrows, Enter, and the mouse wheel.");
    root->append(title);
    root->append(status);

    auto toggle = centui::make_widget<centui::toggle_widget>("Enable notifications", control);
    toggle->set_on_change([status](bool checked)
    {
        status->set_text(checked ? "Notifications enabled" : "Notifications disabled");
    });
    root->append(toggle);

    auto range = centui::make_widget<centui::range_widget>(control, 0.0f, 100.0f, 1.0f);
    range->set_value(50);
    range->set_on_change([status](float value)
    {
        status->set_text(std::format("Volume: {:.0f}%", value));
    });
    root->append(range);

    auto name = centui::make_widget<centui::input_widget>("Edit this name", control);
    name->set_on_change([status](std::string_view value)
    {
        status->set_text(std::format("Name: {}", value));
    });
    root->append(name);

    auto editor_style = control;
    editor_style.height = centui::px(96);
    auto notes = centui::make_widget<centui::input_widget>(
        "Multiline notes\nTry selection, copy/paste, and undo.", editor_style, true);
    root->append(notes);

    auto scroll_style = control;
    scroll_style.height = centui::px(120);
    auto scroll = centui::make_widget<centui::scroll_widget>(scroll_style);
    std::vector<std::string> items;
    for(int index = 1; index <= 12; ++index)
    {
        items.push_back(std::format("Option {}", index));
    }

    auto selection = centui::make_widget<centui::selection_widget>(items);
    selection->set_selected(0);
    selection->set_on_change([status](std::size_t index)
    {
        status->set_text(std::format("Selected option {}", index + 1));
    });
    scroll->append(selection);
    root->append(scroll);

    auto dialog_style = control;
    dialog_style.width = centui::px(360);
    dialog_style.height = centui::px(160);
    dialog_style.gap = centui::px(16);
    auto dialog = centui::make_widget<centui::container_widget>(dialog_style);
    dialog->append(centui::make_widget<centui::text_widget>("A popup composed from ordinary widgets."));
    auto dismiss = centui::make_widget<centui::text_button_widget>("Close", control);
    dialog->append(dismiss);

    centui::style overlay_style;
    overlay_style.align_items = centui::alignment::center;
    overlay_style.justify_content = centui::justification::center;
    overlay_style.background_color = centui::states_color::all({0, 0, 0, 160});
    auto popup = centui::make_widget<centui::popup_widget>(dialog, overlay_style);
    dismiss->set_on_click([weak = popup.weak()]
    {
        if(auto owner = weak.lock(); owner.valid())
        {
            owner->close();
        }
    });

    auto open = centui::make_widget<centui::text_button_widget>("Open popup", control);
    open->set_on_click([popup]
    {
        popup->open();
    });
    root->append(open);

    auto accent_style = control;
    accent_style.height = centui::px(6);
    accent_style.padding = centui::edges::all(centui::px(0));
    accent_style.border_width = 0;
    accent_style.background_color = centui::states_color::all(centui::color::from_hex(0x38BDF8));
    auto accent = centui::make_widget<centui::container_widget>(accent_style);
    root->append(accent);
    root->append(popup);

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

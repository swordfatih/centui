#include <initializer_list>
#include <memory>
#include <string>

#include <SFML/Graphics.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

import centui.widgets;
import centui.sfml;
import centui.types;
import centui.tree;

float display_scale(const sf::RenderWindow& window)
{
#ifdef _WIN32
    const auto dpi = GetDpiForWindow(window.getNativeHandle());
    return dpi > 0 ? static_cast<float>(dpi) / 96.0f : 1.0f;
#else
    // Supply the platform's display preference when embedding this renderer.
    return 1.0f;
#endif
}

struct panel_style
{
    centui::length       width = centui::px(420);
    centui::length       height = centui::px(300);
    centui::edges        padding = centui::edges::all(centui::px(24));
    centui::states_color background_color = centui::states_color::all(centui::color::from_hex(0x243447));
};

struct child_style
{
    centui::length       width = centui::fit();
    centui::length       height = centui::px(100);
    centui::states_color background_color = centui::states_color::all(centui::color::from_hex(0x38BDF8));
};

struct title_style
{
    float                font_size = 32;
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
    } background_color;
};

int main()
{
    sf::RenderWindow window{sf::VideoMode{{960, 540}}, "CentUI with SFML"};
    window.setFramerateLimit(60);

    centui::sfml::backend backend{window, display_scale(window)};
    centui::ui            ui{backend};

    // clang-format off
    auto title = centui::text<title_style>{"Hello, CentUI!"};
    auto action = centui::button<action_button_style>{"Click me", {
        .on_click = [title, count = 0]() mutable
        {
            title.set_text("Button clicked " + std::to_string(++count) + "!");
        },
    }};

    auto root = centui::container<panel_style>{
        title,
        centui::container<child_style>{
            centui::text<title_style>{"This is a nested text..."},
        },
        action,
    };
    // clang-format on

    centui::document document{root};

    while(window.isOpen())
    {
        backend.set_scale(display_scale(window));
        while(const auto event = window.pollEvent())
        {
            if(event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }
            else if(const auto* moved = event->getIf<sf::Event::MouseMoved>())
            {
                ui.process_event(
                    document,
                    {
                        .type = centui::input_event_type::pointer_moved,
                        .x = float(moved->position.x) / backend.viewport().scale,
                        .y = float(moved->position.y) / backend.viewport().scale,
                    });
            }
            else if(const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if(pressed->button == sf::Mouse::Button::Left)
                {
                    ui.process_event(
                        document,
                        {
                            .type = centui::input_event_type::pointer_pressed,
                            .x = float(pressed->position.x) / backend.viewport().scale,
                            .y = float(pressed->position.y) / backend.viewport().scale,
                        });
                }
            }
            else if(const auto* released = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if(released->button == sf::Mouse::Button::Left)
                {
                    ui.process_event(
                        document,
                        {
                            .type = centui::input_event_type::pointer_released,
                            .x = float(released->position.x) / backend.viewport().scale,
                            .y = float(released->position.y) / backend.viewport().scale,
                        });
                }
            }
            else if(event->is<sf::Event::MouseLeft>())
            {
                ui.process_event(document, {.type = centui::input_event_type::pointer_left});
            }
            else if(event->is<sf::Event::FocusLost>())
            {
                ui.process_event(document, {.type = centui::input_event_type::focus_lost});
            }
            else if(event->is<sf::Event::FocusGained>())
            {
                ui.process_event(document, {.type = centui::input_event_type::focus_gained});
            }
        }

        if(!window.isOpen())
        {
            break;
        }

        window.clear(sf::Color{18, 24, 32});
        ui.render(document);
        window.display();
    }
}

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>

#include <SFML/Graphics.hpp>

import centui.widgets;
import centui.sfml;
import centui.types;
import centui.tree;

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

    centui::sfml::platform platform{window};
    centui::sfml::backend backend{window};
    centui::ui ui{platform, backend};

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
    document.set_font(centui::font{CENTUI_FONT_PATH});

    while(window.isOpen())
    {
        while(const auto event = window.pollEvent())
        {
            ui.process_event(document, *event);
            if(event->is<sf::Event::Closed>())
            {
                window.close();
                break;
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

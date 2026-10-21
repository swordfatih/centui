#include <initializer_list>

#include <SFML/Graphics.hpp>

import centui.widgets;
import centui.sfml;
import centui.types;
import centui.tree;

struct panel_style
{
    centui::length       width = centui::px(420);
    centui::length       height = centui::px(180);
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

int main()
{
    ::sf::RenderWindow window{::sf::VideoMode{{960, 540}}, "CentUI with SFML"};
    window.setFramerateLimit(60);

    centui::sfml::backend backend{window};
    centui::ui            ui{backend};

    auto root = centui::container<panel_style>{
        centui::text<title_style>{"Hello, CentUI!"},
        centui::container<child_style>{
            centui::text<title_style>{"This is a nested text..."},
        },
    };
    centui::document document{root};

    while(window.isOpen())
    {
        while(const auto event = window.pollEvent())
        {
            if(event->is<::sf::Event::Closed>())
            {
                window.close();
            }
        }

        window.clear(::sf::Color{18, 24, 32});
        ui.render(document);
        window.display();
    }
}

#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <string>

#include <SFML/Graphics.hpp>

import centui;
import centui.sfml;

namespace
{
struct overlay
{
    centui::length        width = centui::percent(100);
    centui::length        height = centui::percent(100);
    centui::edges         padding = centui::edges::all(centui::px(24));
    centui::alignment     align_items = centui::alignment::end;
    centui::justification justify_content = centui::justification::center;
};

struct panel
{
    centui::length       width = centui::px(300);
    centui::edges        padding = centui::edges::all(centui::px(22));
    centui::length       gap = centui::px(16);
    centui::states_color background_color = centui::states_color::all({24, 34, 48, 235});
};

struct heading
{
    float font_size = 26;
};

struct caption
{
    centui::states_color text_color = centui::states_color::all({170, 185, 204, 255});
};

struct control
{
    centui::length height = centui::px(44);
    centui::edges  padding = centui::edges::all(centui::px(10));
    struct
    {
        centui::color normal{42, 57, 78, 255};
        centui::color hover{53, 77, 106, 255};
        centui::color pressed{31, 47, 66, 255};
        centui::color focused{53, 77, 106, 255};
    } background_color;
};
} // namespace

int main()
{
    sf::RenderWindow window{sf::VideoMode{{800, 520}}, "CentUI + SFML = <3"};
    window.setFramerateLimit(60);

    sf::CircleShape circle{150.f};
    circle.setPosition({60.f, 110.f});
    circle.setFillColor(sf::Color::Green);

    bool visible = true;

    centui::sfml::platform platform{window};
    centui::sfml::backend  renderer{platform};
    centui::ui             ui{platform, renderer};
    auto                   radius = centui::text<caption>{"Radius / 150 px"};

    // clang-format off
    auto root = centui::container<overlay>{
        centui::container<panel>{
            centui::text<caption>{"SFML + CENTUI = <3"},
            centui::text<heading>{"Hello, green circle."},
            centui::text<caption>{"CentUI gives commands. SFML draws."},
            radius,
            centui::range<control>{{
                .minimum = 40,
                .maximum = 1000,
                .step = 1,
                .value = 150,
                .on_change = [&circle, radius](float value) mutable
                {
                    circle.setRadius(value);
                    radius.set_text(std::format("Radius / {:.0f} px", value));
                },
            }},
            centui::toggle<control>{"Show circle", {
                .checked = true,
                .on_change = [&visible](bool value)
                {
                    visible = value;
                },
            }},
        },
    };
    // clang-format on

    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});

    while(window.isOpen())
    {
        while(const auto event = window.pollEvent())
        {
            if(event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }
            ui.process_event(document, *event);
            if(event->is<sf::Event::Resized>())
            {
                const auto size = window.getSize();
                if(size.x && size.y)
                {
                    window.setView(sf::View{sf::FloatRect{{0.f, 0.f}, {static_cast<float>(size.x), static_cast<float>(size.y)}}});
                }
            }
        }

        if(!window.isOpen())
        {
            break;
        }

        window.clear(sf::Color::Black);
        if(visible)
        {
            window.draw(circle);
        }
        ui.render(document);
        window.display();
    }
}

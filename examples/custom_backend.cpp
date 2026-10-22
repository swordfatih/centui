#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <span>
#include <type_traits>
#include <variant>
#include <memory>

import centui.widgets;
import centui.types;
import centui.render;
import centui.tree;

struct panel_style
{
    centui::length width = centui::px(320);
    centui::length height = centui::px(120);
    struct
    {
        centui::color normal = centui::color::from_hex(0x243447);
    } background_color;
};

class logging_backend final : public centui::renderer
{
public:
    [[nodiscard]] centui::viewport viewport() const noexcept override
    {
        return {1600, 1200, 2.0f};
    }

    void draw(const centui::draw_list& list) override
    {
        for(const auto& command: list.commands)
        {
            std::visit([](const auto& item)
            {
                using type = std::decay_t<decltype(item)>;
                if constexpr(std::is_same_v<type, centui::rectangle_command>)
                {
                    std::cout << "rectangle at (" << item.bounds.x << ", " << item.bounds.y << "), size "
                              << item.bounds.width << 'x' << item.bounds.height << '\n';
                }
                else if constexpr(std::is_same_v<type, centui::text_command>)
                {
                    std::cout << "glyphs: " << item.shaped->glyphs.size() << '\n';
                }
                else
                {
                    std::cout << "image: " << item.image->width << 'x' << item.image->height << '\n';
                }
            }, command);
        }
    }

};

int main()
{
    logging_backend backend;
    centui::ui ui{backend};
    centui::container<panel_style> root{};
    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});

    ui.render(document);
}

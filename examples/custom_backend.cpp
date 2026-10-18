import std;

import centui.container;
import centui.color;
import centui.draw;
import centui.glyph;
import centui.unit;
import centui.ui;

struct panel_style
{
    centui::length width = centui::px(320);
    centui::length height = centui::px(120);
    struct
    {
        centui::color normal = centui::color::from_hex(0x243447);
    } background_color;
};

class logging_backend
{
public:
    [[nodiscard]] centui::viewport viewport() const noexcept
    {
        return {800, 600};
    }

    [[nodiscard]] centui::font& glyphs() noexcept
    {
        return _font;
    }

    void draw(std::span<const centui::draw_command> commands)
    {
        for(const auto& command: commands)
        {
            std::visit([](const auto& item)
            {
                using type = std::decay_t<decltype(item)>;
                if constexpr(std::is_same_v<type, centui::rectangle_command>)
                {
                    std::cout << "rectangle at (" << item.bounds.x << ", " << item.bounds.y << "), size "
                              << item.bounds.width << 'x' << item.bounds.height << '\n';
                }
                else
                {
                    std::cout << "text: " << item.value << '\n';
                }
            }, command);
        }
    }

private:
    centui::font _font;
};

int main()
{
    logging_backend backend;
    centui::ui ui{backend};
    centui::container<panel_style> root{};

    ui.render(root);
}

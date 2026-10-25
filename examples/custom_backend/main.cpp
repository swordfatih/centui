#include <iostream>

#include <centui/render.hpp>
#include <centui/types.hpp>
#include <centui/ui.hpp>
#include <centui/widgets.hpp>

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
            std::cout << (command.image ? "textured quad" : "solid quad") << " at (" << command.bounds.x << ", "
                      << command.bounds.y << "), size " << command.bounds.width << 'x' << command.bounds.height << '\n';
        }
    }

    bool begin_frame() override
    {
        return true;
    }

    void end_frame() override
    {
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


export module centui.draw;

import std;
import centui.node;
import centui.color;
import centui.unit;
import centui.glyph;

export namespace centui
{

struct rectangle_command
{
    rect          bounds;
    centui::color color;
};

struct text_command
{
    rect                              bounds;
    std::string_view                  value;
    centui::color                     color;
    float                             font_size;
    std::optional<const text_layout&> shaped;
};

using draw_command = std::variant<rectangle_command, text_command>;

// Commands borrow text and layout until the tree is mutated, moved, or destroyed.
std::vector<draw_command> commands(const node& root)
{
    std::vector<draw_command> result;

    auto visit = [&](this auto&& self, const node& current, float x, float y) -> void
    {
        auto bounds = current.layout.bounds();
        bounds.x += x;
        bounds.y += y;

        auto background_color = current.style.background_color.normal;
        auto text_color = current.style.text_color.normal;
        const std::string* value = nullptr;
        const std::optional<text_layout>* shaped_layout = nullptr;

        if(const auto* text = std::get_if<text_data>(&current.data))
        {
            value = &text->value;
            shaped_layout = &text->shaped;
        }
        else if(const auto* button = std::get_if<details::button_data>(&current.data))
        {
            value = &button->value;
            shaped_layout = &button->shaped;

            const auto& interaction = *button->interaction;
            const auto state_color = [&](const auto& colors)
            {
                if(!interaction.enabled)
                {
                    return colors.disabled;
                }
                if(interaction.pressed)
                {
                    return colors.pressed;
                }
                if(interaction.hovered)
                {
                    return colors.hover;
                }
                if(interaction.focused)
                {
                    return colors.focused;
                }
                return colors.normal;
            };
            background_color = state_color(current.style.background_color);
            text_color = state_color(current.style.text_color);
        }

        if(background_color.a != 0 && bounds.width > 0 && bounds.height > 0)
        {
            result.emplace_back(rectangle_command{bounds, background_color});
        }

        if(value && shaped_layout && text_color.a != 0 && !value->empty())
        {
            const auto shaped = shaped_layout->transform([](const text_layout& value) -> const text_layout&
            {
                return value;
            });
            result.emplace_back(text_command{bounds, *value, text_color, current.style.font_size, shaped});
        }

        for(const auto& child: current.children)
        {
            self(child, bounds.x, bounds.y);
        }
    };

    visit(root, 0.0f, 0.0f);
    return result;
}

} // namespace centui

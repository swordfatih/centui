module;

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

export module centui.draw;

import centui.node;
import centui.model;
import centui.color;
import centui.unit;
import centui.style;
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

namespace details
{

inline void append_paint(const centui::style& style, rect bounds, std::string_view value,
                         const text_layout* shaped, const button_interaction_state* interaction,
                         std::vector<draw_command>& output)
{
    const auto state_color = [interaction](const states_color& colors)
    {
        if(!interaction)
        {
            return colors.normal;
        }
        if(!interaction->enabled)
        {
            return colors.disabled;
        }
        if(interaction->pressed)
        {
            return colors.pressed;
        }
        if(interaction->hovered)
        {
            return colors.hover;
        }
        if(interaction->focused)
        {
            return colors.focused;
        }
        return colors.normal;
    };

    const auto background = state_color(style.background_color);
    if(background.a != 0 && bounds.width > 0 && bounds.height > 0)
    {
        output.emplace_back(rectangle_command{bounds, background});
    }

    const auto foreground = state_color(style.text_color);
    if(foreground.a != 0 && !value.empty())
    {
        std::optional<const text_layout&> prepared;
        if(shaped)
        {
            prepared = *shaped;
        }
        output.emplace_back(text_command{bounds, value, foreground, style.font_size, prepared});
    }
}

inline void append_element_commands(const element_content& content, rect bounds,
                                    const text_layout* shaped, std::vector<draw_command>& output)
{
    std::string_view                value;
    const button_interaction_state* interaction = nullptr;
    if(const auto* text = std::get_if<text_content>(&content.data))
    {
        value = text->value;
    }
    else if(const auto* button = std::get_if<button_content>(&content.data))
    {
        value = button->value;
        interaction = button->interaction.get();
    }
    append_paint(content.style, bounds, value, shaped, interaction, output);
}

inline void append_node_commands(const node& current, rect bounds, std::vector<draw_command>& output)
{
    std::string_view                value;
    const text_layout*              shaped = nullptr;
    const button_interaction_state* interaction = nullptr;
    if(const auto* text = std::get_if<text_data>(&current.data))
    {
        value = text->value;
        shaped = text->shaped ? &*text->shaped : nullptr;
    }
    else if(const auto* button = std::get_if<button_data>(&current.data))
    {
        value = button->value;
        shaped = button->shaped ? &*button->shaped : nullptr;
        interaction = button->interaction.get();
    }
    append_paint(current.style, bounds, value, shaped, interaction, output);
}

} // namespace details

// Compatibility painter for explicitly constructed raw node trees.
std::vector<draw_command> commands(const node& root)
{
    std::vector<draw_command> result;
    auto                      visit = [&](this auto&& self, const node& current, float x, float y) -> void
    {
        auto bounds = current.layout.bounds();
        bounds.x += x;
        bounds.y += y;
        details::append_node_commands(current, bounds, result);
        for(const auto& child: current.children)
        {
            self(child, bounds.x, bounds.y);
        }
    };
    visit(root, 0, 0);
    return result;
}

} // namespace centui

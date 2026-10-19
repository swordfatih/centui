module;

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

export module centui.node;

export import centui.model;
import centui.style;
import centui.unit;
import centui.layout;
import centui.glyph;
import centui.color;

export namespace centui
{

struct text_data
{
    std::string                value;
    std::optional<text_layout> shaped{};
};

namespace details
{

// State for the legacy raw-node API. Runtime input uses document-owned IDs.
struct button_interaction_state
{
    bool                  hovered = false;
    bool                  pressed = false;
    bool                  focused = false;
    bool                  enabled = true;
    std::function<void()> on_click;
};

struct button_data
{
    std::string                               value;
    std::optional<text_layout>                shaped{};
    std::shared_ptr<button_interaction_state> interaction;
};

} // namespace details

struct node
{
    std::vector<node>                                             children{};
    std::variant<container_data, text_data, details::button_data> data = container_data{};
    centui::style                                                 style;
    centui::layout                                                layout{};

    void attach(std::optional<font&> face = std::nullopt)
    {
        validate();
        layout.clear_measure();
        layout.clear_children();

        if(auto* text = std::get_if<text_data>(&data))
        {
            text->shaped.reset();
        }
        if(auto* button = std::get_if<details::button_data>(&data))
        {
            button->shaped.reset();
        }

        update_self(face);
        for(auto& child: children)
        {
            child.attach(face);
            layout.insert_child(child.layout);
        }
    }

    // Explicit synchronization after changing children or node kind.
    void sync_children(std::optional<font&> face = std::nullopt)
    {
        attach(face);
    }

    // Without a font, update styles only. Text/size changes require update(face).
    void update(std::optional<font&> face = std::nullopt)
    {
        validate();
        update_self(face);
        for(auto& child: children)
        {
            child.update(face);
        }
    }

    void calculate(float width, float height)
    {
        layout.calculate(width, height);
    }

private:
    void validate() const
    {
        style.validate();
        if(!std::holds_alternative<container_data>(data) && !children.empty())
        {
            throw std::invalid_argument("Text and button nodes cannot have children");
        }
    }

    void update_self(std::optional<font&> face)
    {
        layout.update(style);
        auto measure_text = [&](std::string_view value, std::optional<text_layout>& shaped)
        {
            if(face)
            {
                auto result = face->layout_text(value, style.font_size);
                layout.set_measure([bounds = result.bounds](float, float)
                {
                    return bounds;
                });
                shaped = std::move(result);
            }
        };

        if(auto* text = std::get_if<text_data>(&data))
        {
            measure_text(text->value, text->shaped);
        }
        else if(auto* button = std::get_if<details::button_data>(&data))
        {
            measure_text(button->value, button->shaped);
        }
    }
};

} // namespace centui

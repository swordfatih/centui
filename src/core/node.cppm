export module centui.node;

import centui.style;
import centui.unit;
import centui.layout;
import centui.glyph;
import std;

export namespace centui
{

struct container_data
{
};

struct text_data
{
    std::string                value;
    std::optional<text_layout> shaped{};
};

struct node
{
    std::vector<node>                       children{};
    std::variant<container_data, text_data> data = container_data{};
    centui::style                           style;
    centui::layout                          layout{};

    void attach(std::optional<font&> face = std::nullopt)
    {
        validate();
        layout.clear_measure();
        layout.clear_children();

        if(auto* text = std::get_if<text_data>(&data))
        {
            text->shaped.reset();
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
        if(std::holds_alternative<text_data>(data) && !children.empty())
        {
            throw std::invalid_argument("Text nodes cannot have children");
        }
    }

    void update_self(std::optional<font&> face)
    {
        layout.update(style);
        if(auto* text = std::get_if<text_data>(&data); text && face)
        {
            auto shaped = face->layout_text(text->value, style.font_size);
            layout.set_measure([bounds = shaped.bounds](float, float)
            {
                return bounds;
            });
            text->shaped = std::move(shaped);
        }
    }
};

} // namespace centui

export module centui.node;

import centui.style;
import centui.unit;
import centui.layout;
import centui.font;
import std;

export namespace centui
{

struct container_data
{
};

struct text_data
{
    std::string value;
};

struct node
{
    std::vector<node>                       children{};
    std::variant<container_data, text_data> data = container_data{};
    centui::style                           style;
    centui::layout                          layout{};

    void attach(font* face = nullptr)
    {
        layout.update(style);

        if(auto* text = std::get_if<text_data>(&data); text && face)
        {
            layout.set_measure([value = text->value, size = style.font_size, face](float, float)
            {
                return face->measure(value, size);
            });
        }

        for(auto& child: children)
        {
            child.attach(face);
            layout.insert_child(child.layout);
        }
    }

    void update()
    {
        layout.update(style);

        for(auto& child: children)
        {
            child.update();
        }
    }

    void calculate(float width, float height)
    {
        layout.calculate(width, height);
    }
};

} // namespace centui
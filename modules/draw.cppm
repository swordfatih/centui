
export module centui.draw;

import std;
import centui.node;
import centui.color;
import centui.unit;

export namespace centui
{

struct rectangle_command
{
    rect          bounds;
    centui::color color;
};

struct text_command
{
    rect             bounds;
    std::string_view value;
    centui::color    color;
    float            font_size;
};

using draw_command = std::variant<rectangle_command, text_command>;

std::vector<draw_command> commands(const node& root)
{
    std::vector<draw_command> result;

    auto visit = [&](this auto&& self, const node& current, float x, float y) -> void
    {
        auto bounds = current.layout.bounds();
        bounds.x += x;
        bounds.y += y;

        if(current.style.background_color.a != 0 && bounds.width > 0 && bounds.height > 0)
        {
            result.emplace_back(rectangle_command{bounds, current.style.background_color});
        }

        if(const auto* text = std::get_if<text_data>(&current.data))
        {
            if(current.style.text_color.a != 0 && !text->value.empty())
            {
                result.emplace_back(text_command{bounds, text->value, current.style.text_color, current.style.font_size});
            }
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

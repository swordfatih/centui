export module centui.node;

import centui.style;
import centui.unit;
import centui.layout;
import std;

export namespace centui
{

struct node
{
    enum class type
    {
        container,
        text,
        button
    };

    std::vector<node> children;
    type              type;
    style             style;
    layout            layout{};

    void attach()
    {
        layout.update(style);

        for(auto& child: children)
        {
            child.attach();
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
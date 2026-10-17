export module centui.node;

import centui.style;
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
};

} // namespace centui
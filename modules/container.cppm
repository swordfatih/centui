
export module centui.container;

import std;
import centui.node;
import centui.style;

export namespace centui
{

template <typename... styles>
struct container
{
public:
    template <typename... children>
    explicit container(children&&... items)
        : node{
              .children = {},
              .data = container_data{},
              .style = style::resolve<styles...>(),
          }
    {
        (node.children.push_back(std::move(items.node)), ...);
    }

    node node;
};

} // namespace centui

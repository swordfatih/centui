
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
              .type = centui::node::type::container,
              .style = centui::style::resolve<styles...>(),
          }
    {
        (node.children.push_back(std::move(items.node)), ...);
    }

    centui::node node;
};

} // namespace centui

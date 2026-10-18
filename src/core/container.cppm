export module centui.container;

import std;
import centui.node;
import centui.style;
export import centui.element;

export namespace centui
{

template <typename... styles>
class container : public element
{
public:
    template <typename... child_types>
        requires((std::derived_from<std::remove_cvref_t<child_types>, element>) && ...)
    explicit container(child_types&&... items)
        : element(node{
              .children = {},
              .data = container_data{},
              .style = style::resolve<styles...>(),
          })
    {
        auto& tree = details::element_access::get(*this);
        tree.children.reserve(sizeof...(child_types));
        (tree.children.push_back(std::move(details::element_access::get(items))), ...);
    }
};

} // namespace centui

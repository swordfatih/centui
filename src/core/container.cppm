module;

#include <initializer_list>
#include <memory>
#include <span>
#include <utility>

export module centui.container;

import centui.node;
import centui.style;
export import centui.element;

export namespace centui
{

template <typename... styles>
class container : public element
{
public:
    static constexpr element_kind widget_kind = element_kind::container;
    container(const container&) = default;
    container& operator=(const container&) = default;
    container(container&&) noexcept = default;
    container& operator=(container&&) noexcept = default;

    container() : container(std::initializer_list<element>{}) {}

    // Braces compose children, even for one child of the same container type.
    explicit container(std::initializer_list<element> items)
        : element(node{
              .children = {},
              .data = container_data{},
              .style = style::resolve<styles...>(),
          })
    {
        compose(std::span<const element>{items.begin(), items.size()});
    }

private:
    friend class element;
    container(details::handle_tag tag, std::shared_ptr<details::element_binding> binding)
        : element(tag, std::move(binding)) {}
};

} // namespace centui

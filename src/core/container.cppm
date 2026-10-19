module;

#include <initializer_list>
#include <span>

export module centui.container;

import centui.model;
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

    container() : container(std::initializer_list<element>{})
    {
    }

    // Braces compose children, even for one child of the same container type.
    explicit container(std::initializer_list<element> items)
        : element(details::element_content{
              .data = container_data{},
              .style = style::resolve<styles...>(),
          })
    {
        compose(std::span<const element>{items.begin(), items.size()});
    }

private:
    friend class element;
    container(details::handle_tag, const element& handle)
        : element(handle)
    {
    }
};

} // namespace centui

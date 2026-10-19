module;

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

export module centui.text;

import centui.node;
import centui.style;
export import centui.element;

export namespace centui
{

template <typename... styles>
class text : public element
{
public:
    static constexpr element_kind widget_kind = element_kind::text;
    explicit text(std::string value)
        : element(node{
              .children = {},
              .data = text_data{std::move(value)},
              .style = style::resolve<styles...>(),
          })
    {
    }

    void value(std::string value)
    {
        std::get<text_data>(details::element_access::get(*this).data).value = std::move(value);
    }

    [[nodiscard]] std::string_view value() const
    {
        return std::get<text_data>(details::element_access::get(*this).data).value;
    }

private:
    friend class element;
    text(details::handle_tag tag, std::shared_ptr<details::element_binding> binding)
        : element(tag, std::move(binding)) {}
};

} // namespace centui

export module centui.text;

import std;
import centui.node;
import centui.style;
export import centui.element;

export namespace centui
{

template <typename... styles>
class text : public element
{
public:
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
};

} // namespace centui

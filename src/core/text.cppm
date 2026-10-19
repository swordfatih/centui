module;

#include <string>
#include <string_view>
#include <utility>
#include <variant>

export module centui.text;

import centui.model;
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
        : element(details::element_content{
              .data = details::text_content{std::move(value)},
              .style = style::resolve<styles...>(),
          })
    {
    }

    void value(std::string value)
    {
        set_text(std::move(value));
    }

    void set_text(std::string value)
    {
        set_text_value(std::move(value));
    }

    [[nodiscard]] std::string_view value() const
    {
        return std::get<details::text_content>(details::element_access::get(*this).data).value;
    }

private:
    friend class element;
    text(details::handle_tag, const element& handle)
        : element(handle)
    {
    }
};

} // namespace centui

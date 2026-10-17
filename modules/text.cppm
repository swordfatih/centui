export module centui.text;

import std;
import centui.node;
import centui.style;

export namespace centui
{

template <typename... styles>
struct text
{
public:
    constexpr explicit text(std::string&& value)
        : node{
              .children = {},
              .type = centui::node::type::text,
              .style = centui::style::resolve<styles...>(),
          },
          value{value}
    {
    }

    centui::node node;
    std::string  value;
};

} // namespace centui
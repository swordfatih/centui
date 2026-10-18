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
              .type = node::type::text,
              .style = style::resolve<styles...>(),
          },
          value{value}
    {
    }

    node node;
    std::string  value;
};

} // namespace centui
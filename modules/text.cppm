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
              .data = text_data{value},
              .style = style::resolve<styles...>(),
          }
    {
    }

    node node;
};

} // namespace centui
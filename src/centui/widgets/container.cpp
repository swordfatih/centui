#include <centui/widgets/container.hpp>

namespace centui
{

container_widget::container_widget(centui::style_source value, std::span<const handle<>> children)
    : widget(std::move(value))
{
    append(children);
}

} // namespace centui

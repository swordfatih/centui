module;

#include <initializer_list>
#include <span>
#include <utility>

export module centui.widgets.container;
export import centui.tree;

export namespace centui
{
class container_widget : public widget
{
public:
    explicit container_widget(centui::style value = {}, std::span<const handle<>> children = {}) : widget(std::move(value))
    {
        append(children);
    }
};

template <typename... Styles>
class container : public handle<container_widget>
{
public:
    container() : container(std::initializer_list<handle<>>{})
    {
    }
    explicit container(centui::style appearance)
        : handle<container_widget>(make_widget<container_widget>(std::move(appearance)))
    {
    }
    // List initialization intentionally nests a single container of the same type.
    explicit container(std::initializer_list<handle<>> children,
                       centui::style appearance = centui::style::resolve<Styles...>())
        : handle<container_widget>(make_widget<container_widget>(
              std::move(appearance), std::span{children.begin(), children.size()}))
    {
    }
};
}

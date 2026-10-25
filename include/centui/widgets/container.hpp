#pragma once

#include <initializer_list>
#include <span>
#include <utility>

#include <centui/widget.hpp>

namespace centui
{

/// \brief Groups children and participates in document layout.
class container_widget : public widget
{
public:
    explicit container_widget(centui::style_source value = {}, std::span<const handle<>> children = {});
};

/// \brief A container handle with compile-time style composition.
template <typename... Styles>
class container : public handle<container_widget>
{
public:
    container()
        : container(std::initializer_list<handle<>>{})
    {
    }

    explicit container(centui::style_source appearance)
        : handle<container_widget>(
              make_widget<container_widget>(centui::style_source::with_composition<Styles...>(std::move(appearance)))
          )
    {
    }

    // List initialization intentionally nests a single container of the same type.
    explicit container(std::initializer_list<handle<>> children, centui::style_source appearance = {})
        : handle<container_widget>(make_widget<container_widget>(
              centui::style_source::with_composition<Styles...>(std::move(appearance)),
              std::span{children.begin(), children.size()}
          ))
    {
    }
};

} // namespace centui

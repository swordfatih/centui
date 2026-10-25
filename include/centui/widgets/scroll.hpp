#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <utility>

#include <centui/widgets/container.hpp>

namespace centui
{

/// \brief Clips its children and keeps their scroll offset within content bounds.
class scroll_widget : public container_widget
{
public:
    explicit scroll_widget(centui::style_source appearance = {});

    [[nodiscard]] vec2 offset() const noexcept;

    [[nodiscard]] vec2 limit() const noexcept;

    void set_offset(vec2 value);

protected:
    void on_geometry() override;

    bool on_event(const input_event& event) override;

private:
    vec2 m_offset{};
    vec2 m_limit{};
};

/// \brief A scroll handle with compile-time style composition.
template <typename... Styles>
class scroll : public handle<scroll_widget>
{
public:
    scroll()
        : scroll(std::initializer_list<handle<>>{})
    {
    }

    explicit scroll(std::initializer_list<handle<>> children, centui::style_source appearance = {})
        : handle<scroll_widget>(
              make_widget<scroll_widget>(centui::style_source::with_composition<Styles...>(std::move(appearance)))
          )
    {
        this->get().append(std::span{children.begin(), children.size()});
    }
};

} // namespace centui

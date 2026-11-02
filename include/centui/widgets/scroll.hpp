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

struct scrollbar_options
{
    float thickness = 10;
    float minimum_thumb = 24;
    float thumb_inset = 2;
    float corner_radius = 3;

    bool operator==(const scrollbar_options&) const = default;
};

/// \brief Clips children, clamps scrolling, and paints draggable overlay scrollbars when content overflows.
class scroll_widget : public container_widget
{
public:
    explicit scroll_widget(centui::style_source appearance = {});

    [[nodiscard]] vec2 offset() const noexcept;

    [[nodiscard]] vec2 limit() const noexcept;

    void set_offset(vec2 value);

    void set_scrollbars(scrollbar_options options);

    [[nodiscard]] scrollbar_options scrollbars() const noexcept;

protected:
    bool can_scroll(vec2 delta) const override;
    vec2 reveal_descendant(rect target) override;

    void on_geometry() override;

    void paint_overlay(painter& output) const override;

    [[nodiscard]] bool hit_test_overlay(vec2 position) const override;

    bool on_event(const input_event& event) override;

private:
    enum class axis
    {
        none,
        horizontal,
        vertical
    };

    struct scrollbar
    {
        rect track;
        rect thumb;
    };

    [[nodiscard]] scrollbar bar(axis direction) const noexcept;

    vec2 m_offset{};
    vec2 m_limit{};
    axis m_drag_axis = axis::none;
    float m_drag_grab = 0;
    scrollbar_options m_scrollbar;
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

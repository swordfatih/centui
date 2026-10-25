#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

#include <centui/widget.hpp>
#include <centui/widgets/callback.hpp>

namespace centui
{

/// \brief Initial value, bounds, step, and change callback.
struct range_options
{
    float minimum = 0;
    float maximum = 1;
    float step = 0.01f;
    float value = 0;
    centui::callback<float> on_change{};
};

/// \brief Selects a stepped numeric value with pointer or keyboard input.
class range_widget : public widget
{
public:
    explicit range_widget(range_options options = {}, centui::style_source appearance = {});

    void set_range(float minimum, float maximum, float step);

    void set_value(float value);

    [[nodiscard]] float value() const noexcept;

    void set_on_change(centui::callback<float> callback);

protected:
    size measure(const measure_context&) override;

    void paint(painter& output) const override;

    bool on_event(const input_event& event) override;

private:
    void change(float value);

    float m_minimum = 0;
    float m_maximum = 1;
    float m_step = 0.01f;
    float m_value = 0;
    bool m_dragging = false;
    centui::callback<float> m_on_change;
};

/// \brief A numeric range handle with compile-time style composition.
template <typename... Styles>
class range : public handle<range_widget>
{
public:
    explicit range(range_options options = {}, centui::style_source appearance = {})
        : handle<range_widget>(make_widget<range_widget>(
              std::move(options),
              centui::style_source::with_composition<Styles...>(std::move(appearance))
          ))
    {
    }
};

} // namespace centui

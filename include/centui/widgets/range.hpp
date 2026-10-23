#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

#include <centui/tree.hpp>
#include <centui/widgets/callback.hpp>

namespace centui
{

/// \brief Selects a stepped numeric value with pointer or keyboard input.
class range_widget : public widget
{
public:
    explicit range_widget(centui::style appearance = {}, float minimum = 0, float maximum = 1, float step = 0.01f);

    void set_range(float minimum, float maximum, float step);

    void set_value(float value);

    [[nodiscard]] float value() const noexcept;

    void set_on_change(std::function<void(float)> callback);

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
    details::callback<float> m_on_change;
};

/// \brief Initial value, bounds, step, and change callback.
struct range_options
{
    float minimum = 0;
    float maximum = 1;
    float step = 0.01f;
    float value = 0;
    std::function<void(float)> on_change{};
};

/// \brief A numeric range handle with compile-time style composition.
template <typename... Styles>
class range : public handle<range_widget>
{
public:
    explicit range(range_options options = {}, centui::style appearance = centui::style::resolve<Styles...>())
        : handle<range_widget>(
              make_widget<range_widget>(std::move(appearance), options.minimum, options.maximum, options.step)
          )
    {
        this->get().set_value(options.value);
        this->get().set_on_change(std::move(options.on_change));
    }
};

} // namespace centui

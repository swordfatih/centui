#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <centui/widgets/pressable.hpp>
#include <centui/widgets/text.hpp>

namespace centui
{

using button_options = pressable_options;

/// \brief A pressable control with a text label.
class button_widget : public pressable_widget
{
public:
    explicit button_widget(std::string value, button_options options = {}, centui::style_source appearance = {});

    void set_text(std::string value);

    [[nodiscard]] std::string_view value() const noexcept;

private:
    handle<text_widget> m_text;
};

/// \brief A button handle with compile-time style composition.
template <typename... Styles>
class button : public handle<button_widget>
{
public:
    explicit button(std::string value, button_options options = {}, centui::style_source appearance = {})
        : handle<button_widget>(make_widget<button_widget>(
              std::move(value),
              std::move(options),
              centui::style_source::with_composition<Styles...>(std::move(appearance))
          ))
    {
    }
};

} // namespace centui

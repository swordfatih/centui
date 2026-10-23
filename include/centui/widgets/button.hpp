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
    explicit button_widget(std::string value, centui::style appearance = {}, button_options options = {});

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
    explicit button(
        std::string value,
        button_options options = {},
        centui::style appearance = centui::style::resolve<Styles...>()
    )
        : handle<button_widget>(make_widget<button_widget>(std::move(value), std::move(appearance), std::move(options)))
    {
    }

    void set_text(std::string value)
    {
        this->get().set_text(std::move(value));
    }

    void value(std::string value)
    {
        set_text(std::move(value));
    }

    [[nodiscard]] std::string_view value() const
    {
        return this->get().value();
    }

    void set_enabled(bool value)
    {
        this->get().set_enabled(value);
    }

    [[nodiscard]] bool enabled() const
    {
        return this->get().enabled();
    }

    void set_on_click(std::function<void()> callback)
    {
        this->get().set_on_click(std::move(callback));
    }
};

} // namespace centui

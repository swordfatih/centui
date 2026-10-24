#pragma once

#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <utility>

#include <centui/widgets/callback.hpp>
#include <centui/widgets/container.hpp>

namespace centui
{

/// \brief Initial enabled state and activation callback.
struct pressable_options
{
    bool enabled = true;
    std::function<void()> on_click{};
};

/// \brief Activates from pointer or keyboard input and owns its content.
class pressable_widget : public container_widget
{
public:
    explicit pressable_widget(pressable_options options = {}, centui::style appearance = {});

    explicit pressable_widget(handle<> content, pressable_options options = {}, centui::style appearance = {});

    void set_on_click(std::function<void()> callback);

protected:
    virtual void activate();

    bool on_event(const input_event& event) override;

private:
    details::callback<> m_on_click;
    bool m_armed = false;
};

/// \brief A pressable handle with compile-time style composition.
template <typename... Styles>
class pressable : public handle<pressable_widget>
{
public:
    pressable()
        : pressable(std::initializer_list<handle<>>{})
    {
    }

    explicit pressable(
        handle<> content,
        pressable_options options = {},
        centui::style appearance = centui::style::resolve<Styles...>()
    )
        : handle<pressable_widget>(
              make_widget<pressable_widget>(std::move(content), std::move(options), std::move(appearance))
          )
    {
    }

    explicit pressable(
        std::initializer_list<handle<>> children,
        pressable_options options = {},
        centui::style appearance = centui::style::resolve<Styles...>()
    )
        : handle<pressable_widget>(make_widget<pressable_widget>(std::move(options), std::move(appearance)))
    {
        this->get().append(std::span{children.begin(), children.size()});
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

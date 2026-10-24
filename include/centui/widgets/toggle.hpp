#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <centui/widgets/button.hpp>
#include <centui/widgets/callback.hpp>

namespace centui
{

/// \brief Initial checked state and change callback.
struct toggle_options
{
    bool checked = false;
    std::function<void(bool)> on_change{};
};

/// \brief A labeled control that toggles its checked state on activation.
class toggle_widget : public button_widget
{
public:
    explicit toggle_widget(std::string label, toggle_options options = {}, centui::style appearance = {});

    [[nodiscard]] bool checked() const noexcept;

    void set_checked(bool value);

    void set_on_change(std::function<void(bool)> callback);

protected:
    void activate() override;

    void paint(painter& output) const override;

private:
    bool m_checked = false;
    details::callback<bool> m_on_change;
};

/// \brief A toggle handle with compile-time style composition.
template <typename... Styles>
class toggle : public handle<toggle_widget>
{
public:
    explicit toggle(
        std::string label,
        toggle_options options = {},
        centui::style appearance = centui::style::resolve<Styles...>()
    )
        : handle<toggle_widget>(make_widget<toggle_widget>(std::move(label), std::move(options), std::move(appearance)))
    {
    }
};

} // namespace centui

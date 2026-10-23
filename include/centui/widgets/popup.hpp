#pragma once

#include <functional>
#include <memory>
#include <utility>

#include <centui/widgets/callback.hpp>
#include <centui/widgets/container.hpp>

namespace centui
{

/// \brief Shows overlay content and restores the previous focus when closed.
class popup_widget : public container_widget
{
public:
    explicit popup_widget(handle<> content, centui::style appearance = {});

    /// \brief Shows the popup and requests focus inside its content.
    void open();

    /// \brief Hides the popup and restores the previous focus when it is still available.
    void close();

    void set_on_close(std::function<void()> callback);

protected:
    void on_layout() override;

    bool on_event(const input_event& event) override;

private:
    static handle<> find_focus(const handle<>& root);

    bool m_focus_pending = false;
    weak_handle<> m_previous_focus;
    details::callback<> m_on_close;
};

/// \brief A popup handle with compile-time style composition.
template <typename... Styles>
class popup : public handle<popup_widget>
{
public:
    explicit popup(handle<> content, centui::style appearance = centui::style::resolve<Styles...>())
        : handle<popup_widget>(make_widget<popup_widget>(std::move(content), std::move(appearance)))
    {
    }
};

} // namespace centui

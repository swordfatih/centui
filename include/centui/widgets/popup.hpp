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
    explicit popup_widget(handle<> content, centui::style_source appearance = {});

    /// \brief Shows the popup and requests focus inside its content.
    /// Falls back to the popup when its content has no eligible focus target.
    void open();

    /// \brief Hides the popup and restores the previous focus when it is still available.
    void close();

    void set_on_close(centui::callback<> callback);

protected:
    bool on_event(const input_event& event) override;

private:
    bool m_open_requested = false;
    centui::callback<> m_on_close;
};

/// \brief A popup handle with compile-time style composition.
template <typename... Styles>
class popup : public handle<popup_widget>
{
public:
    explicit popup(handle<> content, centui::style_source appearance = {})
        : handle<popup_widget>(make_widget<popup_widget>(
              std::move(content),
              centui::style_source::with_composition<Styles...>(std::move(appearance))
          ))
    {
    }
};

} // namespace centui

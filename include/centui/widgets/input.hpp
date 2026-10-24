#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <centui/widget.hpp>
#include <centui/widgets/callback.hpp>

namespace centui
{

/// \brief Initial editing mode and text callbacks.
struct input_options
{
    bool multiline = false;
    std::function<void(std::string_view)> on_change{};

    std::function<void()> on_submit{};
};

/// \brief Editable UTF-8 text with selection, composition, and undo history.
class input_widget : public widget
{
public:
    explicit input_widget(std::string value = {}, input_options options = {}, centui::style appearance = {});

    [[nodiscard]] std::string_view value() const noexcept;

    void set_text(std::string value);

    /// \brief Selects a UTF-8 byte range, clamped to valid character boundaries.
    void set_selection(std::size_t anchor, std::size_t caret);

    [[nodiscard]] std::pair<std::size_t, std::size_t> selection() const noexcept;

    void set_on_change(std::function<void(std::string_view)> callback);

    void set_on_submit(std::function<void()> callback);

protected:
    size measure(const measure_context& context) override;

    baseline_function baseline() override;

    void on_layout() override;

    void paint(painter& output) const override;

    std::optional<rect> text_input_area() const override;

    void on_focus_changed(bool value) override;

    bool on_event(const input_event& event) override;

private:
    struct snapshot
    {
        std::string value;
        std::size_t anchor;
        std::size_t caret;
    };

    std::string normalize(std::string value) const;

    std::size_t boundary(std::size_t offset) const noexcept;

    std::size_t adjacent(bool forward) const;

    std::size_t hit(float x, float y) const;

    void move_caret(std::size_t next, bool extend);

    std::size_t display_caret() const noexcept;

    void prepare_text(text_service& service, float width) const;

    void changed_layout();

    void notify_change();

    void replace_selection(std::string replacement);

    void restore(std::vector<snapshot>& source, std::vector<snapshot>& destination);

    std::string m_value;
    std::string m_preedit;

    std::size_t m_anchor = 0;
    std::size_t m_caret = 0;
    std::size_t m_preedit_caret = 0;

    bool m_multiline = false;
    bool m_dragging = false;
    bool m_reveal_caret = true;
    vec2 m_scroll{};

    std::vector<snapshot> m_undo;
    std::vector<snapshot> m_redo;

    details::callback<std::string_view> m_on_change;
    details::callback<> m_on_submit;

    mutable std::shared_ptr<const text_layout> m_layout;
    mutable text_cache m_cache;
};

/// \brief A text input handle with compile-time style composition.
template <typename... Styles>
class input : public handle<input_widget>
{
public:
    explicit input(
        std::string value = {},
        input_options options = {},
        centui::style appearance = centui::style::resolve<Styles...>()
    )
        : handle<input_widget>(make_widget<input_widget>(std::move(value), std::move(options), std::move(appearance)))
    {
    }
};

} // namespace centui

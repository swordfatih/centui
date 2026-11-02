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

#include <centypo/editor.hpp>

namespace centui
{

/// \brief Initial editing mode and text callbacks.
struct input_options
{
    bool multiline = false;
    centui::callback<std::string_view> on_change{};

    centui::callback<> on_submit{};
};

/// \brief Editable UTF-8 text with selection, composition, and undo history.
class input_widget : public widget
{
public:
    explicit input_widget(std::string value = {}, input_options options = {}, centui::style_source appearance = {});

    ~input_widget() override;

    [[nodiscard]] std::string_view value() const noexcept;

    void set_text(std::string value);

    /// \brief Selects a UTF-8 byte range, clamped backwards to extended grapheme boundaries.
    void set_selection(std::size_t anchor, std::size_t caret);
    void set_selection(centypo::text_position anchor, centypo::text_position caret);
    [[nodiscard]] std::pair<centypo::text_position, centypo::text_position> directional_selection() const noexcept;

    [[nodiscard]] std::pair<std::size_t, std::size_t> selection() const noexcept;

    void set_on_change(centui::callback<std::string_view> callback);

    void set_on_submit(centui::callback<> callback);

protected:
    size measure(const measure_context& context) override;

    baseline_function baseline() override;

    void on_geometry() override;

    void paint(painter& output) const override;

    std::optional<rect> text_input_area() const override;

    void on_focus_activity_changed(bool value) override;

    bool on_event(const input_event& event) override;
    void on_service_result(const service_result& result) override;
    bool multiline_text_input() const override;

private:
    centypo::text_position hit(float x, float y) const;

    void move_caret(centypo::text_position next, bool extend);

    void prepare_text(const centypo::engine& service, float width) const;

    void refresh_text_geometry();

    void changed_layout();

    void notify_change();

    void replace_selection(std::string replacement, bool typing = false);

    centypo::editor m_editor;
    std::optional<float> m_preferred_x;

    bool m_multiline = false;
    bool m_dragging = false;
    bool m_reveal_caret = true;
    vec2 m_scroll{};

    centui::callback<std::string_view> m_on_change;
    centui::callback<> m_on_submit;

    mutable std::optional<centypo::layout> m_layout;
    std::uint64_t m_edit_revision = 0;
    std::uint64_t m_request_revision = 0;
    request_id m_pending_request = 0;
    bool m_cut_request = false;
};

/// \brief A text input handle with compile-time style composition.
template <typename... Styles>
class input : public handle<input_widget>
{
public:
    explicit input(std::string value = {}, input_options options = {}, centui::style_source appearance = {})
        : handle<input_widget>(make_widget<input_widget>(
              std::move(value),
              std::move(options),
              centui::style_source::with_composition<Styles...>(std::move(appearance))
          ))
    {
    }
};

} // namespace centui

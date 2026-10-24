#pragma once

#include <chrono>
#include <span>

#include <centui/handle.hpp>
#include <centui/layout.hpp>
#include <centui/painter.hpp>
#include <centui/types.hpp>

namespace centui
{

using frame_duration = std::chrono::duration<float>;

/// \brief Constraints and font services available while measuring a widget.
struct measure_context
{
    axis_constraint width;
    axis_constraint height;
    text_service& text;
};

namespace details
{

struct document_context;

}

/// \brief Base class for widgets owned by a document tree.
/// Parents own children; parent and document links are weak.
/// Widget and document access is confined to one application thread.
class widget : public std::enable_shared_from_this<widget>
{
public:
    virtual ~widget() = default;

    widget(const widget&) = delete;

    widget& operator=(const widget&) = delete;

    [[nodiscard]] const centui::style& style() const noexcept;

    [[nodiscard]] centui::style resolved_style() const;

    [[nodiscard]] rect content_bounds() const noexcept;

    [[nodiscard]] const layout_metrics& metrics() const noexcept;

    [[nodiscard]] bool hovered() const noexcept;

    [[nodiscard]] bool pressed() const noexcept;

    [[nodiscard]] bool focused() const noexcept;

    [[nodiscard]] bool effectively_enabled() const noexcept;

    void append(const handle<>& child);

    void append(std::span<const handle<>> children);

    void remove(const handle<>& child);

    [[nodiscard]] std::vector<handle<>> children() const;

    [[nodiscard]] handle<> parent() const;

    /// \brief Requests focus; tree changes made during input callbacks are applied after dispatch.
    bool request_focus();

    /// \brief Moves focus within the current scope; reverse walks the scope backward.
    bool focus_next(bool reverse = false);

    [[nodiscard]] bool effectively_visible() const noexcept;

    void set_style(centui::style replacement);

    void patch_style(const style_patch& patch);

    [[nodiscard]] rect bounds() const noexcept;

    [[nodiscard]] bool enabled() const noexcept;

    void set_enabled(bool value);

protected:
    explicit widget(centui::style value = {});

    /// \brief Measures content without mutating the document or its widgets.
    virtual size measure(const measure_context&);

    /// \brief Supplies an optional content baseline callback; it must not mutate the tree.
    virtual baseline_function baseline();

    /// \brief Records drawing commands without mutating the document or its widgets.
    virtual void paint(painter&) const;

    /// \brief Handles an input event and returns whether it was consumed.
    virtual bool on_event(const input_event&);

    virtual void on_focus_changed(bool);

    virtual void on_layout();

    /// \brief Runs once after request_frame(); request again to continue animation.
    virtual void on_frame(frame_duration elapsed);

    /// \brief Coalesces requests for the next document tick, without retaining this widget.
    void request_frame();

    virtual std::optional<rect> text_input_area() const;

    const host_services& host() const;

    text_service& fonts();

    [[nodiscard]] vec2 child_offset() const noexcept;

    void set_focus_scope(bool value = true);

    void set_interactive(bool value = true);

    void set_focusable(bool value = true);

    void set_clip_children(bool value);

    void set_child_offset(vec2 value);

    void check_mutation() const;

    /// \brief Requests new geometry and paint after a content change.
    void invalidate_layout();

    /// \brief Requests new paint while retaining the current geometry.
    void invalidate_paint();

private:
    friend class document;
    friend struct details::widget_access;

    void interaction_changed();

    void validate_children(std::span<const handle<>> children) const;

    void insert_children(std::span<const handle<>> children);

    centui::style m_style;
    rect m_bounds{};
    rect m_content{};
    layout_metrics m_metrics{};
    std::vector<std::shared_ptr<widget>> m_children;
    std::weak_ptr<widget> m_parent;

    std::shared_ptr<const int> m_identity = std::make_shared<const int>(0);

    std::weak_ptr<const int> m_parent_claim;
    std::weak_ptr<details::document_context> m_document;
    bool m_interactive = false;
    bool m_focusable = false;
    bool m_focus_scope = false;
    bool m_clip_children = false;
    vec2 m_child_offset{};
    bool m_enabled = true;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_focused = false;
    bool m_measure_dirty = true;
    bool m_frame_requested = false;
};

} // namespace centui

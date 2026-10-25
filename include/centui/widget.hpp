#pragma once

#include <chrono>
#include <span>

#include <centui/event.hpp>
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
struct invalidation_access;

} // namespace details

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

    /// \brief Makes every property locally explicit, retaining the composition for reset.
    void set_style(centui::style replacement);

    /// \brief Retains supplied properties as final local overrides.
    void patch_style(const style_patch& patch);

    /// \brief Removes final local overrides and resolves the retained composition.
    void clear_style_overrides();

    [[nodiscard]] rect bounds() const noexcept;

    [[nodiscard]] bool enabled() const noexcept;

    void set_enabled(bool value);

    /// Opt-in command reuse while geometry and inherited appearance stay unchanged.
    void set_paint_boundary(bool value);

protected:
    explicit widget(centui::style_source value = {});

    /// \brief Measures content without mutating widgets or requesting work. Derived caches are permitted.
    virtual size measure(const measure_context&);

    /// \brief Supplies a baseline callback; neither evaluation nor this hook may mutate or request work.
    virtual baseline_function baseline();

    /// \brief Records commands without mutating widgets or requesting work. Derived caches are permitted.
    virtual void paint(painter&) const;

    /// \brief Handles an input event and returns whether it was consumed.
    virtual bool on_event(const input_event&);

    virtual void on_focus_changed(bool);

    /// Rebuild geometry from current bounds; also called for explicit geometry requests.
    /// Hooks may edit widgets, but must converge. Intrinsic changes require invalidate_layout().
    virtual void on_geometry();

    /// \brief Runs once after request_frame(); request again to continue animation.
    virtual void on_frame(frame_duration elapsed);

    /// \brief Coalesces ticks without retaining this widget; the callback must invalidate what it changes.
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

    /// \brief Content/measurement or baseline inputs changed; calculate layout, refresh geometry and paint.
    void invalidate_layout();

    /// Rebuild local geometry without Yoga calculation; includes paint invalidation.
    void invalidate_geometry();

    /// \brief Requests new paint while retaining the current geometry.
    void invalidate_paint();

private:
    friend class document;
    friend class details::event_access;
    friend struct details::widget_access;
    friend struct details::invalidation_access;

    void interaction_changed();

    void validate_children(std::span<const handle<>> children) const;

    void insert_children(std::span<const handle<>> children);

    [[nodiscard]] centui::style resolve_style(
        const style_patch& defaults,
        const details::style_patch_map& patches,
        details::style_patch_reference local
    ) const;

    [[nodiscard]] details::style_patch_reference local_style() const noexcept;

    bool commit_style(centui::style value, bool notify = true);

    centui::style m_style;
    std::reference_wrapper<const details::style_recipe> m_style_recipe;
    std::unique_ptr<style_patch> m_style_overrides;
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
    bool m_paint_boundary = false;
    std::size_t m_paint_revision = 0;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_focused = false;
    bool m_layout_dirty = true;
    bool m_layout_descendants_dirty = true;
    bool m_geometry_dirty = true;
    bool m_frame_requested = false;
};

} // namespace centui

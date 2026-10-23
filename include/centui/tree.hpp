#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <centui/font.hpp>
#include <centui/layout.hpp>
#include <centui/platform.hpp>
#include <centui/render.hpp>
#include <centui/style.hpp>

namespace centui
{
namespace details
{

[[nodiscard]] centui::color state_color(const states_color& colors, visual_state state) noexcept;

} // namespace details

/// \brief Records widget drawing in document coordinates within the inherited clip.
class painter
{
public:
    painter(
        draw_list& output,
        text_service& fonts,
        rect content,
        rect clip,
        const centui::style& style,
        details::visual_state state = details::visual_state::normal
    );

    [[nodiscard]] rect bounds() const noexcept;

    [[nodiscard]] text_service& fonts() const noexcept;

    [[nodiscard]] centui::color foreground() const noexcept;

    void rectangle(rect bounds, centui::color color);

    void text(std::shared_ptr<const text_layout> layout);

    void image(std::shared_ptr<const bitmap> value, rect bounds, centui::color tint = {255, 255, 255, 255});

    void text(std::string_view value);

    void text(
        std::shared_ptr<const text_layout> layout,
        rect bounds,
        centui::color color,
        text_alignment alignment = text_alignment::left
    );

private:
    draw_list& m_output;
    text_service& m_fonts;
    rect m_bounds;
    rect m_clip;
    const centui::style& m_style;
    details::visual_state m_state;
};

class widget;
class document;

template <typename T = widget>
class handle;

template <typename T = widget>
class weak_handle;

/// \brief Constraints and font services available while measuring a widget.
struct measure_context
{
    axis_constraint width;
    axis_constraint height;
    text_service& text;
};

namespace details
{

struct document_context
{
    explicit document_context(document& value);

    bool layout_dirty = true;
    bool paint_dirty = true;
    bool preparing = false;
    // The document owns this context; widgets only keep weak references to it.
    document& owner;

    void check_mutation() const;
};
struct widget_access;

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

    [[nodiscard]] const centui::style& appearance() const noexcept;

    [[nodiscard]] centui::style resolved_style() const;

    [[nodiscard]] rect content_bounds() const noexcept;

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

    /// \brief Records drawing commands without mutating the document or its widgets.
    virtual void paint(painter&) const;

    /// \brief Handles an input event and returns whether it was consumed.
    virtual bool on_event(const input_event&);

    virtual void on_focus_changed(bool);

    virtual void on_layout();

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
};

/// \brief Shared, typed access to a widget. Empty handles throw on dereference.
template <typename T>
class handle
{
public:
    handle() = default;

    explicit handle(std::shared_ptr<T> value)
        : m_value(std::move(value))
    {
    }

    template <typename U>
        requires std::derived_from<U, T>
    handle(const handle<U>& other)
        : m_value(other.m_value)
    {
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return static_cast<bool>(m_value);
    }

    [[nodiscard]] T& get() const
    {
        if(!m_value)
        {
            throw std::logic_error("Access through an empty widget handle");
        }

        return *m_value;
    }

    [[nodiscard]] const std::shared_ptr<T>& operator->() const
    {
        (void)get();

        return m_value;
    }

    [[nodiscard]] T& operator*() const
    {
        return get();
    }

    [[nodiscard]] const centui::style& style() const
    {
        return get().style();
    }

    void set_style(centui::style value)
    {
        get().set_style(std::move(value));
    }

    void patch_style(const style_patch& patch)
    {
        get().patch_style(patch);
    }

    [[nodiscard]] weak_handle<T> weak() const noexcept
    {
        return weak_handle<T>{*this};
    }

    [[nodiscard]] bool operator==(const handle&) const noexcept = default;

private:
    template <typename>
    friend class handle;

    template <typename>
    friend class weak_handle;
    friend class document;
    friend class widget;
    friend struct details::widget_access;

    std::shared_ptr<T> m_value;
};

/// \brief Observes a widget without extending its lifetime.
/// Use weak handles when callbacks capture their own widget or an ancestor.
template <typename T>
class weak_handle
{
public:
    weak_handle() = default;

    weak_handle(const handle<T>& value)
        : m_value(value.m_value)
    {
    }

    [[nodiscard]] handle<T> lock() const noexcept
    {
        return handle<T>{m_value.lock()};
    }

private:
    std::weak_ptr<T> m_value;
};

namespace details
{

struct widget_access
{
    static void finish(const std::shared_ptr<widget>& parent);
};

} // namespace details

template <typename T, typename... Args>
    requires std::derived_from<T, widget> && (!std::is_abstract_v<T>) && std::constructible_from<T, Args...>
[[nodiscard]] handle<T> make_widget(Args&&... args)
{
    auto value = std::make_shared<T>(std::forward<Args>(args)...);

    details::widget_access::finish(value);

    return handle<T>{std::move(value)};
}

/// \brief Owns a widget tree and coordinates layout, painting, focus, and input.
class document
{
public:
    document();

    explicit document(handle<> root);

    ~document();

    document(const document&) = delete;

    document& operator=(const document&) = delete;

    document(document&&) = delete;

    document& operator=(document&&) = delete;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] handle<> root() const noexcept;

    [[nodiscard]] text_service& fonts() noexcept;

    void set_font(const centui::font& face);

    void set_host_services(std::shared_ptr<const host_services> services);

    [[nodiscard]] invalidation pending_changes() const noexcept;

    void mount(handle<> value);

    /// \brief Replaces the mounted tree with a detached root.
    void replace_root(handle<> value);

    void set_root(handle<> value);

    [[nodiscard]] std::vector<handle<>> children(const handle<>& parent) const;

    [[nodiscard]] handle<> parent(const handle<>& child) const;

    void append(const handle<>& parent, const handle<>& child);

    void append(const handle<>& parent, std::span<const handle<>> children);

    void remove(const handle<>& child);

    /// \brief Returns a borrowed draw list, valid until the next preparation or destruction.
    [[nodiscard]] const draw_list& prepare(centui::viewport viewport);

    void process_event(const input_event& event, centui::viewport viewport);

    /// \brief Dispatches portable events in order, then applies deferred tree changes.
    void process_events(std::span<const input_event> events, centui::viewport viewport);

private:
    friend class widget;

    void interaction_changed();

    bool request_focus(const handle<>& target);

    bool advance_focus(const handle<>& scope, bool reverse);

    void collect_focus(const std::shared_ptr<widget>& value, std::vector<handle<>>& result) const;

    void sync_host();

    template <typename Operation>
    void dispatch_guarded(Operation&& operation)
    {
        m_context->check_mutation();

        if(m_dispatching)
        {
            throw std::logic_error("Recursive event dispatch is not allowed");
        }

        m_dispatching = true;

        try
        {
            operation();
            settle_input(m_viewport);
            sync_host();
            m_dispatching = false;
        }
        catch(...)
        {
            const auto failure = std::current_exception();

            try
            {
                do
                {
                    flush_edits();
                    synchronize_input();
                } while(!m_edits.empty());
            }
            catch(...)
            {
                // Callback exceptions retain priority over failures applying edits.
            }

            m_dispatching = false;

            std::rethrow_exception(failure);
        }
    }

    struct phase_guard
    {
        details::document_context& context;

        explicit phase_guard(details::document_context& value);

        ~phase_guard();
    };

    struct layout_entry
    {
        std::shared_ptr<widget> value;
        centui::layout native;
        std::vector<std::unique_ptr<layout_entry>> children;
    };

    static void associate(
        const std::shared_ptr<widget>& value,
        const std::shared_ptr<details::document_context>& context
    ) noexcept;

    [[nodiscard]] std::shared_ptr<widget> checked(const handle<>& value) const;

    void validate_root(const handle<>& value) const;

    void validate_append(const handle<>& parent, std::span<const handle<>> children) const;

    void dirty_tree() noexcept;

    void edit(std::function<void()> operation);

    void flush_edits();

    void settle_input(centui::viewport viewport);

    std::unique_ptr<layout_entry> make_layout(const std::shared_ptr<widget>& value);

    static void check_measurements(layout_entry& entry);

    static void store_bounds(layout_entry& entry, float x, float y);

    void update_layout(centui::viewport viewport);

    void notify_layout(const std::shared_ptr<widget>& value);

    void paint_tree(
        const widget& value,
        draw_list& output,
        rect clip,
        bool enabled,
        details::visual_state inherited_state = details::visual_state::normal
    );

    static bool contains(rect box, float x, float y) noexcept;

    [[nodiscard]] bool eligible(const std::shared_ptr<widget>& value) const;

    [[nodiscard]] bool focus_eligible(const std::shared_ptr<widget>& value) const;

    void set_focus(std::shared_ptr<widget> value);

    std::shared_ptr<widget> hit(const std::shared_ptr<widget>& value, float x, float y, rect clip) const;

    std::shared_ptr<widget> hit(float x, float y) const;

    // Uncaptured motion may fall back to an ancestor. Gestures remain target-only.
    void route_motion(std::shared_ptr<widget> target, const input_event& event);

    bool route_event(std::shared_ptr<widget> target, const input_event& event);

    void set_hover(std::shared_ptr<widget> value);

    void cancel_capture();

    void clear_input();

    void synchronize_input();

    void dispatch(const input_event& event);

    handle<> m_root;
    std::shared_ptr<details::document_context> m_context;
    text_service m_fonts;
    std::shared_ptr<const void> m_font_revision;
    draw_list m_draw;

    centui::viewport m_viewport{};

    std::weak_ptr<widget> m_hover;
    std::weak_ptr<widget> m_capture;
    mouse_button m_capture_button = mouse_button::primary;
    std::weak_ptr<widget> m_focus;

    std::vector<std::function<void()>> m_edits;
    bool m_dispatching = false;
    bool m_host_focused = true;
    bool m_pointer_known = false;
    bool m_hit_dirty = true;
    float m_pointer_x = 0;
    float m_pointer_y = 0;

    std::shared_ptr<const host_services> m_host;
    std::optional<rect> m_text_input_area;
};

/// \brief Connects a document to borrowed platform and renderer instances.
template <typename Platform = centui::platform>
class ui
{
public:
    explicit ui(renderer& backend)
        : m_backend(backend)
    {
    }

    ui(Platform& host, renderer& backend)
        : m_backend(backend),
          m_platform(host)
    {
    }

    void render(document& owner)
    {
        attach(owner);

        const auto viewport = surface();
        const auto& list = owner.prepare(viewport);

        if(viewport.width > 0 && viewport.height > 0)
        {
            m_backend.draw(list);
        }
    }

    void process_event(document& owner, const input_event& event)
    {
        attach(owner);
        owner.process_event(event, surface());
    }

    template <typename Event>
        requires requires(Platform& host, const Event& event) { host.translate(event); }
    void process_event(document& owner, const Event& event)
    {
        if(!m_platform)
        {
            throw std::logic_error("Native events require a platform");
        }

        if(auto translated = m_platform->get().translate(event))
        {
            process_event(owner, *translated);
        }
    }

    void process_events(document& owner, std::span<const input_event> events)
    {
        attach(owner);
        owner.process_events(events, surface());
    }

private:
    void attach(document& owner)
    {
        if(m_platform)
        {
            owner.set_host_services(m_platform->get().services());
        }
    }

    centui::viewport surface() const
    {
        return m_platform ? m_platform->get().viewport() : m_backend.viewport();
    }

    renderer& m_backend;
    std::optional<std::reference_wrapper<Platform>> m_platform;
};

template <typename Platform, typename Renderer>
void run(Platform& host, Renderer& backend, document& owner)
{
    ui integration{host, backend};

    while(host.poll_events())
    {
        integration.process_events(owner, host.events());

        if(backend.begin_frame())
        {
            integration.render(owner);
            backend.end_frame();
        }
    }
}

} // namespace centui

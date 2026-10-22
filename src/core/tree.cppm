module;

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

export module centui.tree;
export import centui.platform;

export import centui.style;
export import centui.render;
export import centui.font;
import centui.layout;

export namespace centui
{
namespace details
{
[[nodiscard]] inline centui::color state_color(const states_color& colors, visual_state state) noexcept
{
    switch(state)
    {
    case visual_state::disabled:
    {
        return colors.disabled;
    }
    case visual_state::pressed:
    {
        return colors.pressed;
    }
    case visual_state::hovered:
    {
        return colors.hover;
    }
    case visual_state::focused:
    {
        return colors.focused;
    }
    default:
    {
        return colors.normal;
    }
    }
}

} // namespace details

// A short-lived widget painting context. Bounds use document coordinates.
// Framework traversal supplies translated bounds and the inherited clip.
class painter
{
public:
    painter(draw_list& output, text_service& fonts, rect content, rect clip,
            const centui::style& style, details::visual_state state = details::visual_state::normal)
        : _output(output), _fonts(fonts), _bounds(content), _clip(clip), _style(style), _state(state)
    {
    }

    [[nodiscard]] rect bounds() const noexcept
    {
        return _bounds;
    }

    [[nodiscard]] text_service& fonts() const noexcept
    {
        return _fonts;
    }

    [[nodiscard]] centui::color foreground() const noexcept
    {
        return details::state_color(_style.text_color, _state);
    }

    void rectangle(rect bounds, centui::color color)
    {
        const auto clip = intersection(bounds, _clip);
        if(color.a != 0 && clip.width > 0 && clip.height > 0)
        {
            _output.commands.emplace_back(rectangle_command{bounds, color, _clip});
        }
    }

    void text(std::shared_ptr<const text_layout> layout)
    {
        text(std::move(layout), _bounds, details::state_color(_style.text_color, _state), _style.text_align);
    }

    void image(std::shared_ptr<const bitmap> value, rect bounds, centui::color tint = {255, 255, 255, 255})
    {
        if(value)
        {
            validate_bitmap(*value);
            _output.commands.emplace_back(image_command{bounds, _clip, std::move(value), tint});
        }
    }

    void text(std::string_view value)
    {
        if(!value.empty())
        {
            text(_fonts.prepare(value, _style.font, _style.font_size, std::max(0.0f, _bounds.width)));
        }
    }

    void text(std::shared_ptr<const text_layout> layout, rect bounds, centui::color color,
              text_alignment alignment = text_alignment::left)
    {
        const auto clip = intersection(bounds, _clip);
        if(layout && color.a != 0 && clip.width > 0 && clip.height > 0)
        {
            _output.commands.emplace_back(text_command{bounds, color, std::move(layout), alignment, clip});
        }
    }

private:
    draw_list&            _output;
    text_service&         _fonts;
    rect                  _bounds;
    rect                  _clip;
    const centui::style&  _style;
    details::visual_state _state;
};

} // namespace centui

export namespace centui
{

class widget;
class document;
template <typename T = widget>
class handle;
template <typename T = widget>
class weak_handle;

struct measure_context
{
    axis_constraint width;
    axis_constraint height;
    text_service&   text;
};

namespace details
{
struct document_context
{
    explicit document_context(document& value) : owner(value)
    {
    }

    bool layout_dirty = true;
    bool paint_dirty = true;
    bool preparing = false;
    // The document owns this context; widgets only keep weak references to it.
    document& owner;

    void check_mutation() const
    {
        if(preparing)
        {
            throw std::logic_error("Cannot mutate widgets during measurement or painting");
        }
    }
};
struct widget_access;
} // namespace details

// Widgets, documents and font services are confined to one application thread.
// Parents own children; parent and document links never prolong their lifetimes.
class widget : public std::enable_shared_from_this<widget>
{
public:
    virtual ~widget() = default;
    widget(const widget&) = delete;
    widget& operator=(const widget&) = delete;

    [[nodiscard]] const centui::style& style() const noexcept
    {
        return _style;
    }

    [[nodiscard]] const centui::style& appearance() const noexcept
    {
        return _style;
    }

    [[nodiscard]] centui::style resolved_style() const
    {
        auto result = _style;
        for(auto ancestor = _parent.lock(); !result.font && ancestor; ancestor = ancestor->_parent.lock())
        {
            result.font = ancestor->_style.font;
        }
        return result;
    }

    [[nodiscard]] rect content_bounds() const noexcept
    {
        return _content;
    }
    [[nodiscard]] bool hovered() const noexcept
    {
        return _hovered;
    }
    [[nodiscard]] bool pressed() const noexcept
    {
        return _pressed;
    }
    [[nodiscard]] bool focused() const noexcept
    {
        return _focused;
    }
    [[nodiscard]] bool effectively_enabled() const noexcept
    {
        if(!_enabled)
        {
            return false;
        }
        for(auto parent = _parent.lock(); parent; parent = parent->_parent.lock())
        {
            if(!parent->_enabled)
            {
                return false;
            }
        }
        return true;
    }

    void                                append(const handle<>& child);
    void                                append(std::span<const handle<>> children);
    void                                remove(const handle<>& child);
    [[nodiscard]] std::vector<handle<>> children() const;
    [[nodiscard]] handle<>              parent() const;
    // Returns whether the request was accepted. Requests made in callbacks are deferred.
    bool request_focus();
    bool focus_next(bool reverse = false);

    [[nodiscard]] bool effectively_visible() const noexcept
    {
        if(_style.visibility != visibility::visible)
        {
            return false;
        }

        for(auto ancestor = _parent.lock(); ancestor; ancestor = ancestor->_parent.lock())
        {
            if(ancestor->_style.visibility != visibility::visible)
            {
                return false;
            }
        }

        return true;
    }

    void set_style(centui::style replacement)
    {
        check_mutation();
        replacement.validate();
        const auto changes = replacement.changes_from(_style);
        if(!changes.layout && !changes.paint)
        {
            return;
        }
        const bool targeting_changed = _style.visibility != replacement.visibility || _style.overflow != replacement.overflow;
        _style = std::move(replacement);
        if(changes.layout)
        {
            invalidate_layout();
        }
        else
        {
            invalidate_paint();
        }

        if(targeting_changed)
        {
            interaction_changed();
        }
    }

    void patch_style(const style_patch& patch)
    {
        set_style(patch.apply(_style));
    }

    [[nodiscard]] rect bounds() const noexcept
    {
        return _bounds;
    }

    [[nodiscard]] bool enabled() const noexcept
    {
        return _enabled;
    }

    void set_enabled(bool value)
    {
        check_mutation();
        if(_enabled == value)
        {
            return;
        }
        _enabled = value;
        if(!value)
        {
            _hovered = false;
            _pressed = false;
        }
        invalidate_paint();
        interaction_changed();
    }

protected:
    explicit widget(centui::style value = {}) : _style(std::move(value))
    {
        _style.validate();
    }

    virtual size measure(const measure_context&)
    {
        return {};
    }

    virtual void paint(painter&) const
    {
    }

    virtual bool on_event(const input_event&)
    {
        return false;
    }

    virtual void on_focus_changed(bool)
    {
    }

    virtual void on_layout()
    {
    }

    virtual std::optional<rect> text_input_area() const
    {
        return std::nullopt;
    }

    const host_services& host() const;
    text_service& fonts();

    [[nodiscard]] vec2 child_offset() const noexcept
    {
        return _child_offset;
    }

    void set_focus_scope(bool value = true)
    {
        check_mutation();
        _focus_scope = value;
    }

    void set_interactive(bool value = true)
    {
        check_mutation();
        if(_interactive == value)
        {
            return;
        }
        _interactive = value;
        interaction_changed();
    }

    void set_focusable(bool value = true)
    {
        check_mutation();
        if(_focusable == value)
        {
            return;
        }
        _focusable = value;
        interaction_changed();
    }

    void set_clip_children(bool value)
    {
        check_mutation();
        if(_clip_children == value)
        {
            return;
        }
        _clip_children = value;
        invalidate_paint();
        interaction_changed();
    }

    void set_child_offset(vec2 value)
    {
        check_mutation();
        if(!std::isfinite(value.x) || !std::isfinite(value.y))
        {
            throw std::invalid_argument("Child offsets must be finite");
        }
        if(_child_offset == value)
        {
            return;
        }
        _child_offset = value;
        invalidate_layout();
    }

    void check_mutation() const
    {
        if(auto context = _document.lock())
        {
            context->check_mutation();
        }
    }

    void invalidate_layout()
    {
        check_mutation();
        if(auto context = _document.lock())
        {
            context->layout_dirty = true;
            context->paint_dirty = true;
        }
    }

    void invalidate_paint()
    {
        check_mutation();
        if(auto context = _document.lock())
        {
            context->paint_dirty = true;
        }
    }

private:
    friend class document;
    friend struct details::widget_access;
    void                                     interaction_changed();
    void                                     validate_children(std::span<const handle<>> children) const;
    void                                     insert_children(std::span<const handle<>> children);
    centui::style                            _style;
    rect                                     _bounds{};
    rect                                     _content{};
    std::vector<std::shared_ptr<widget>>     _children;
    std::weak_ptr<widget>                    _parent;
    std::shared_ptr<const int>               _identity = std::make_shared<const int>(0);
    std::weak_ptr<const int>                 _parent_claim;
    std::weak_ptr<details::document_context> _document;
    bool                                     _interactive = false;
    bool                                     _focusable = false;
    bool                                     _focus_scope = false;
    bool                                     _clip_children = false;
    vec2                                     _child_offset{};
    bool                                     _enabled = true;
    bool                                     _hovered = false;
    bool                                     _pressed = false;
    bool                                     _focused = false;
};

// Empty and moved-from handles throw std::logic_error on access. Copies alias.
template <typename T>
class handle
{
public:
    handle() = default;
    explicit handle(std::shared_ptr<T> value) : _value(std::move(value))
    {
    }
    template <typename U>
        requires std::derived_from<U, T>
    handle(const handle<U>& other) : _value(other._value)
    {
    }
    [[nodiscard]] bool valid() const noexcept
    {
        return static_cast<bool>(_value);
    }
    [[nodiscard]] T& get() const
    {
        if(!_value)
        {
            throw std::logic_error("Access through an empty widget handle");
        }
        return *_value;
    }
    [[nodiscard]] T* operator->() const
    {
        return std::addressof(get());
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
    std::shared_ptr<T> _value;
};

// Callbacks that capture themselves or an ancestor must capture a weak handle.
template <typename T>
class weak_handle
{
public:
    weak_handle() = default;
    weak_handle(const handle<T>& value) : _value(value._value)
    {
    }
    [[nodiscard]] handle<T> lock() const noexcept
    {
        return handle<T>{_value.lock()};
    }

private:
    std::weak_ptr<T> _value;
};

namespace details
{
struct widget_access
{
    static void finish(const std::shared_ptr<widget>& parent)
    {
        for(const auto& child: parent->_children)
        {
            child->_parent = parent;
        }
    }
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

void widget::validate_children(std::span<const handle<>> children) const
{
    std::vector<std::shared_ptr<widget>> seen;
    seen.reserve(children.size());
    for(const auto& child: children)
    {
        (void)child.get();
        auto value = child._value;
        if(value.get() == this || !value->_parent_claim.expired() || !value->_document.expired() ||
           std::ranges::find(seen, value) != seen.end())
        {
            throw std::invalid_argument("A child must be distinct, detached and unmounted");
        }
        for(auto ancestor = _parent.lock(); ancestor; ancestor = ancestor->_parent.lock())
        {
            if(ancestor == value)
            {
                throw std::invalid_argument("Child insertion would create a cycle");
            }
        }
        seen.push_back(value);
    }
}

void widget::insert_children(std::span<const handle<>> children)
{
    _children.reserve(_children.size() + children.size());
    for(const auto& child: children)
    {
        child._value->_parent_claim = _identity;
        child._value->_parent = weak_from_this();
        _children.push_back(child._value);
    }
}

} // namespace centui

export namespace centui
{
class document
{
public:
    document() : _context(std::make_shared<details::document_context>(*this))
    {
    }
    explicit document(handle<> root) : document()
    {
        replace_root(std::move(root));
    }
    ~document()
    {
        associate(_root._value, {});
    }
    document(const document&) = delete;
    document& operator=(const document&) = delete;
    document(document&&) = delete;
    document& operator=(document&&) = delete;

    [[nodiscard]] bool empty() const noexcept
    {
        return !_root.valid();
    }
    [[nodiscard]] handle<> root() const noexcept
    {
        return _root;
    }
    [[nodiscard]] text_service& fonts() noexcept
    {
        return _fonts;
    }

    void set_font(const centui::font& face)
    {
        _context->check_mutation();
        const auto revision = _fonts.identity();
        _fonts.set_font(face);
        if(revision != _fonts.identity())
        {
            dirty_tree();
        }
    }

    void set_host_services(std::shared_ptr<const host_services> services)
    {
        _context->check_mutation();
        if(_host == services)
        {
            return;
        }

        if(_host && _host->text_input)
        {
            _host->text_input(std::nullopt);
        }

        _host = std::move(services);
        _text_input_area.reset();
        sync_host();
    }
    [[nodiscard]] invalidation pending_changes() const noexcept
    {
        return {.layout = _context->layout_dirty, .paint = _context->paint_dirty};
    }
    void mount(handle<> value)
    {
        if(!empty())
        {
            throw std::logic_error("Document already has a root");
        }
        replace_root(std::move(value));
    }
    void replace_root(handle<> value)
    {
        _context->check_mutation();
        validate_root(value);
        edit([this, value = std::move(value)]
        {
            validate_root(value);
            if(value == _root)
            {
                return;
            }
            clear_input();
            associate(_root._value, {});
            _root = value;
            associate(_root._value, _context);
            dirty_tree();
        });
    }
    void set_root(handle<> value)
    {
        replace_root(std::move(value));
    }
    [[nodiscard]] std::vector<handle<>> children(const handle<>& parent) const
    {
        return checked(parent)->children();
    }
    [[nodiscard]] handle<> parent(const handle<>& child) const
    {
        return handle<>{checked(child)->_parent.lock()};
    }
    void append(const handle<>& parent, const handle<>& child)
    {
        append(parent, std::span{std::addressof(child), std::size_t{1}});
    }
    void append(const handle<>& parent, std::span<const handle<>> children)
    {
        _context->check_mutation();
        validate_append(parent, children);
        std::vector<handle<>> batch{children.begin(), children.end()};
        edit([this, parent, batch = std::move(batch)]
        {
            validate_append(parent, batch);
            auto target = checked(parent);
            target->insert_children(batch);
            for(const auto& child: batch)
            {
                associate(child._value, _context);
            }
            dirty_tree();
        });
    }
    void remove(const handle<>& child)
    {
        _context->check_mutation();
        (void)checked(child);
        edit([this, child]
        {
            auto target = checked(child);
            if(target == _root._value)
            {
                _root = {};
            }
            else
            {
                auto parent = target->_parent.lock();
                std::erase(parent->_children, target);
                target->_parent.reset();
                target->_parent_claim.reset();
            }
            associate(target, {});
            dirty_tree();
            synchronize_input();
        });
    }

    // The returned list is borrowed until the next preparation or destruction.
    [[nodiscard]] const draw_list& prepare(centui::viewport viewport)
    {
        _context->check_mutation();
        process_events({}, viewport);
        if(viewport.width <= 0 || viewport.height <= 0)
        {
            _draw.commands.clear();
            _draw.viewport = viewport;
            return _draw;
        }
        if(_context->paint_dirty)
        {
            phase_guard guard{*_context};
            draw_list   candidate{viewport, {}};
            if(_root.valid())
            {
                paint_tree(*_root._value, candidate, {0, 0, viewport.logical_width(), viewport.logical_height()}, true);
            }
            _draw = std::move(candidate);
            _font_revision = _fonts.identity();
            _context->paint_dirty = false;
        }
        return _draw;
    }
    void process_event(const input_event& event, centui::viewport viewport)
    {
        process_events(std::span{std::addressof(event), std::size_t{1}}, viewport);
    }
    void process_events(std::span<const input_event> events, centui::viewport viewport)
    {
        dispatch_guarded([&]
        {
            settle_input(viewport);
            for(const auto& event: events)
            {
                dispatch(event);
                settle_input(viewport);
            }
        });
    }

private:
    friend class widget;

    void interaction_changed()
    {
        _hit_dirty = true;
        if(_dispatching)
        {
            synchronize_input();
        }
        else
        {
            dispatch_guarded([this]
            {
                synchronize_input();
            });
        }
    }

    bool request_focus(const handle<>& target)
    {
        _context->check_mutation();
        if(!focus_eligible(checked(target)))
        {
            return false;
        }
        edit([this, target]
        {
            if(focus_eligible(target._value))
            {
                set_focus(target._value);
            }
        });
        return true;
    }

    bool advance_focus(const handle<>& scope, bool reverse)
    {
        std::vector<handle<>> candidates;
        collect_focus(checked(scope), candidates);
        if(candidates.empty())
        {
            return false;
        }

        const auto found = std::ranges::find(candidates, handle<>{_focus.lock()});
        std::size_t index = reverse ? candidates.size() - 1 : 0;
        if(found != candidates.end())
        {
            index = static_cast<std::size_t>(found - candidates.begin());
            index = reverse ? (index + candidates.size() - 1) % candidates.size() : (index + 1) % candidates.size();
        }

        return request_focus(candidates[index]);
    }

    void collect_focus(const std::shared_ptr<widget>& value, std::vector<handle<>>& result) const
    {
        if(!value || !value->effectively_enabled() || !value->effectively_visible())
        {
            return;
        }

        if(focus_eligible(value))
        {
            result.emplace_back(value);
        }

        for(const auto& child: value->_children)
        {
            collect_focus(child, result);
        }
    }

    void sync_host()
    {
        const auto focused = _focus.lock();
        const auto area = focused ? focused->text_input_area() : std::nullopt;
        if(area != _text_input_area)
        {
            _text_input_area = area;
            if(_host && _host->text_input)
            {
                _host->text_input(area);
            }
        }
    }

    template <typename Operation>
    void dispatch_guarded(Operation&& operation)
    {
        _context->check_mutation();
        if(_dispatching)
        {
            throw std::logic_error("Recursive event dispatch is not allowed");
        }
        _dispatching = true;
        try
        {
            operation();
            settle_input(_viewport);
            sync_host();
            _dispatching = false;
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
                } while(!_edits.empty());
            }
            catch(...)
            {
                // Callback exceptions retain priority over failures applying edits.
            }
            _dispatching = false;
            std::rethrow_exception(failure);
        }
    }

    struct phase_guard
    {
        details::document_context& context;
        explicit phase_guard(details::document_context& value) : context(value)
        {
            context.check_mutation();
            context.preparing = true;
        }
        ~phase_guard()
        {
            context.preparing = false;
        }
    };
    struct layout_entry
    {
        std::shared_ptr<widget>                    value;
        centui::layout                             native;
        std::vector<std::unique_ptr<layout_entry>> children;
    };
    handle<>                                   _root;
    std::shared_ptr<details::document_context> _context;
    text_service                               _fonts;
    std::shared_ptr<const void>                _font_revision;
    draw_list                                  _draw;
    centui::viewport                           _viewport{};
    std::weak_ptr<widget>                      _hover;
    std::weak_ptr<widget>                      _capture;
    mouse_button                               _capture_button = mouse_button::primary;
    std::weak_ptr<widget>                      _focus;
    std::vector<std::function<void()>>         _edits;
    bool                                       _dispatching = false;
    bool                                       _host_focused = true;
    bool                                       _pointer_known = false;
    bool                                       _hit_dirty = true;
    float                                      _pointer_x = 0;
    float                                      _pointer_y = 0;
    std::shared_ptr<const host_services> _host;
    std::optional<rect> _text_input_area;

    static void associate(const std::shared_ptr<widget>& value, const std::shared_ptr<details::document_context>& context) noexcept
    {
        if(!value)
        {
            return;
        }
        value->_document = context;
        if(!context)
        {
            value->_hovered = false;
            value->_pressed = false;
            value->_focused = false;
        }
        for(const auto& child: value->_children)
        {
            associate(child, context);
        }
    }
    [[nodiscard]] std::shared_ptr<widget> checked(const handle<>& value) const
    {
        (void)value.get();
        if(value._value->_document.lock() != _context)
        {
            throw std::invalid_argument("Widget does not belong to this document");
        }
        return value._value;
    }
    void validate_root(const handle<>& value) const
    {
        if(!value.valid() || value == _root)
        {
            return;
        }
        if(!value->_parent_claim.expired() || !value->_document.expired())
        {
            throw std::invalid_argument("Document root must be detached and unmounted");
        }
    }
    void validate_append(const handle<>& parent, std::span<const handle<>> children) const
    {
        checked(parent)->validate_children(children);
    }
    void dirty_tree() noexcept
    {
        _hit_dirty = true;
        _context->layout_dirty = true;
        _context->paint_dirty = true;
    }
    void edit(std::function<void()> operation)
    {
        if(_dispatching)
        {
            _edits.push_back(std::move(operation));
        }
        else
        {
            dispatch_guarded(operation);
        }
    }
    void flush_edits()
    {
        std::exception_ptr failure;
        while(!_edits.empty())
        {
            auto pending = std::exchange(_edits, {});
            for(auto& operation: pending)
            {
                try
                {
                    operation();
                }
                catch(...)
                {
                    if(!failure)
                    {
                        failure = std::current_exception();
                    }
                }
            }
        }
        if(failure)
        {
            std::rethrow_exception(failure);
        }
    }
    void settle_input(centui::viewport viewport)
    {
        do
        {
            flush_edits();
            update_layout(viewport);
            synchronize_input();
        } while(!_edits.empty() || (_context->layout_dirty && viewport.width > 0 && viewport.height > 0));
    }
    std::unique_ptr<layout_entry> make_layout(const std::shared_ptr<widget>& value)
    {
        auto entry = std::make_unique<layout_entry>();
        entry->value = value;
        entry->native.update(value->_style);
        if(value->_children.empty())
        {
            entry->native.set_measure([this, value](axis_constraint width, axis_constraint height)
            {
                const auto result = value->measure({width, height, _fonts});
                if(!std::isfinite(result.width) || !std::isfinite(result.height) || result.width < 0 || result.height < 0)
                {
                    throw std::invalid_argument("Widget measurement must be finite and nonnegative");
                }
                return result;
            });
        }
        else
        {
            for(const auto& child: value->_children)
            {
                auto layout = make_layout(child);
                entry->native.insert_child(layout->native);
                entry->children.push_back(std::move(layout));
            }
        }
        return entry;
    }
    static void check_measurements(layout_entry& entry)
    {
        entry.native.rethrow_measure_failure();
        for(auto& child: entry.children)
        {
            check_measurements(*child);
        }
    }
    static void store_bounds(layout_entry& entry, float x, float y)
    {
        auto box = entry.native.bounds();
        box.x += x;
        box.y += y;
        entry.value->_bounds = box;
        auto content = entry.native.content_bounds();
        content.x += box.x;
        content.y += box.y;
        entry.value->_content = content;
        for(auto& child: entry.children)
        {
            store_bounds(*child, box.x + entry.value->_child_offset.x,
                         box.y + entry.value->_child_offset.y);
        }
    }
    void update_layout(centui::viewport viewport)
    {
        if(!std::isfinite(viewport.scale) || viewport.scale <= 0)
        {
            throw std::invalid_argument("Viewport scale must be finite and positive");
        }
        _fonts.set_scale(viewport.scale);

        if(_font_revision != _fonts.identity())
        {
            dirty_tree();
        }
        if(viewport != _viewport)
        {
            if(viewport.scale != _viewport.scale)
            {
                _pointer_x *= _viewport.scale / viewport.scale;
                _pointer_y *= _viewport.scale / viewport.scale;
                _text_input_area.reset();
            }
            _viewport = viewport;
            dirty_tree();
        }
        if(!_context->layout_dirty || viewport.width <= 0 || viewport.height <= 0)
        {
            return;
        }
        {
            phase_guard guard{*_context};
            if(_root.valid())
            {
                auto tree = make_layout(_root._value);
                tree->native.calculate(viewport.logical_width(), viewport.logical_height());
                check_measurements(*tree);
                store_bounds(*tree, 0, 0);
            }
        }
        _context->layout_dirty = false;
        _hit_dirty = true;
        _context->paint_dirty = true;
        _font_revision = _fonts.identity();
        notify_layout(_root._value);
    }

    void notify_layout(const std::shared_ptr<widget>& value)
    {
        if(!value)
        {
            return;
        }

        value->on_layout();
        for(const auto& child: value->_children)
        {
            notify_layout(child);
        }
    }

    void paint_tree(const widget& value, draw_list& output, rect clip, bool enabled)
    {
        if(value._style.visibility != visibility::visible)
        {
            return;
        }

        enabled = enabled && value._enabled;
        const auto state = !enabled ? details::visual_state::disabled : value._pressed ? details::visual_state::pressed
                                                                    : value._hovered   ? details::visual_state::hovered
                                                                    : value._focused   ? details::visual_state::focused
                                                                                       : details::visual_state::normal;
        const auto appearance = value.resolved_style();
        painter background{output, _fonts, value._bounds, clip, appearance, state};
        background.rectangle(value._bounds, details::state_color(value._style.background_color, state));
        const auto border = details::state_color(value._style.border_color, state);
        const auto width = std::min(value._style.border_width, std::min(value._bounds.width, value._bounds.height) / 2);
        if(width > 0)
        {
            const auto box = value._bounds;
            background.rectangle({box.x, box.y, box.width, width}, border);
            background.rectangle({box.x, box.y + box.height - width, box.width, width}, border);
            background.rectangle({box.x, box.y + width, width, box.height - 2 * width}, border);
            background.rectangle({box.x + box.width - width, box.y + width, width, box.height - 2 * width}, border);
        }
        painter content{output, _fonts, value._content, intersection(clip, value._content), appearance, state};
        value.paint(content);
        const auto child_clip = value._clip_children || value._style.overflow != overflow::visible ? intersection(clip, value._content) : clip;
        for(const auto& child: value._children)
        {
            paint_tree(*child, output, child_clip, enabled);
        }
    }
    static bool contains(rect box, float x, float y) noexcept
    {
        return x >= box.x && y >= box.y && x < box.x + box.width && y < box.y + box.height;
    }
    [[nodiscard]] bool eligible(const std::shared_ptr<widget>& value) const
    {
        if(!value || !_host_focused || _viewport.width <= 0 || _viewport.height <= 0 ||
           value->_document.lock() != _context || !value->_interactive)
        {
            return false;
        }
        return value->effectively_enabled() && value->effectively_visible();
    }
    [[nodiscard]] bool focus_eligible(const std::shared_ptr<widget>& value) const
    {
        return value && _host_focused && value->_document.lock() == _context &&
               value->_focusable && value->effectively_enabled() && value->effectively_visible();
    }
    void set_focus(std::shared_ptr<widget> value)
    {
        auto old = _focus.lock();
        if(old == value)
        {
            return;
        }
        _focus.reset();
        _context->paint_dirty = true;
        if(old)
        {
            old->_focused = false;
            old->on_focus_changed(false);
        }
        if(focus_eligible(value))
        {
            _focus = value;
            value->_focused = true;
            value->on_focus_changed(true);
        }
    }
    std::shared_ptr<widget> hit(const std::shared_ptr<widget>& value, float x, float y, rect clip) const
    {
        if(!value || !value->_enabled || value->_style.visibility != visibility::visible || !contains(clip, x, y))
        {
            return {};
        }
        const auto child_clip = value->_clip_children || value->_style.overflow != overflow::visible ? intersection(clip, value->_content) : clip;
        for(auto child = value->_children.rbegin(); child != value->_children.rend(); ++child)
        {
            if(auto target = hit(*child, x, y, child_clip))
            {
                return target;
            }
        }
        return eligible(value) && contains(value->_bounds, x, y) ? value : nullptr;
    }
    std::shared_ptr<widget> hit(float x, float y) const
    {
        return hit(_root._value, x, y,
                   {0, 0, _viewport.logical_width(), _viewport.logical_height()});
    }
    // Uncaptured motion may fall back to an ancestor. Gestures remain target-only.
    void route_motion(std::shared_ptr<widget> target, const input_event& event)
    {
        for(auto current = std::move(target); current; current = current->_parent.lock())
        {
            if(eligible(current) && current->on_event(event))
            {
                return;
            }
        }
    }
    bool route_event(std::shared_ptr<widget> target, const input_event& event)
    {
        for(auto current = std::move(target); current; current = current->_parent.lock())
        {
            if(current->effectively_enabled() && current->effectively_visible() && current->on_event(event))
            {
                return true;
            }
        }

        return false;
    }

    void set_hover(std::shared_ptr<widget> value)
    {
        if(auto old = _hover.lock(); old != value)
        {
            if(old)
            {
                old->_hovered = false;
            }
            if(value)
            {
                value->_hovered = true;
            }
            _hover = value;
            _context->paint_dirty = true;
        }
    }
    void cancel_capture()
    {
        auto captured = _capture.lock();
        _capture.reset();
        if(captured)
        {
            captured->_pressed = false;
            _context->paint_dirty = true;
            captured->on_event({.type = input_event_type::pointer_cancelled, .button = _capture_button});
        }
    }
    void clear_input()
    {
        set_hover({});
        cancel_capture();
        set_focus({});
    }
    void synchronize_input()
    {
        if(!eligible(_hover.lock()))
        {
            set_hover({});
        }
        if(!eligible(_capture.lock()))
        {
            cancel_capture();
        }
        if(auto focused = _focus.lock(); focused && !focus_eligible(focused))
        {
            set_focus({});
        }
        if(_hit_dirty && !_context->layout_dirty)
        {
            _hit_dirty = false;
            const bool inside = _pointer_known && _host_focused &&
                                _pointer_x >= 0 && _pointer_y >= 0 &&
                                _pointer_x < _viewport.logical_width() && _pointer_y < _viewport.logical_height();
            auto       target = inside ? hit(_pointer_x, _pointer_y) : nullptr;
            set_hover(target);
            if(auto captured = _capture.lock())
            {
                const bool pressed = captured == target;
                if(captured->_pressed != pressed)
                {
                    captured->_pressed = pressed;
                    _context->paint_dirty = true;
                }
            }
        }
    }
    void dispatch(const input_event& event)
    {
        if(event.type == input_event_type::key_pressed || event.type == input_event_type::key_released ||
           event.type == input_event_type::text_entered || event.type == input_event_type::composition)
        {
            if(!_host_focused)
            {
                return;
            }

            if(!route_event(_focus.lock(), event) && event.type == input_event_type::key_pressed && event.code == key::tab)
            {
                auto scope = _root._value;
                for(auto current = _focus.lock(); current; current = current->_parent.lock())
                {
                    if(current->_focus_scope)
                    {
                        scope = current;
                        break;
                    }
                }

                if(scope)
                {
                    advance_focus(handle<>{scope}, event.mods.shift);
                }
            }

            return;
        }

        _hit_dirty = true;
        switch(event.type)
        {
        case input_event_type::focus_lost:
        {
            _pointer_known = false;
            _host_focused = false;
            clear_input();
            return;
        }
        case input_event_type::focus_gained:
        {
            _host_focused = true;
            return;
        }
        case input_event_type::pointer_left:
        {
            _pointer_known = false;
            set_hover({});
            if(auto captured = _capture.lock())
            {
                captured->_pressed = false;
                _context->paint_dirty = true;
            }
            return;
        }
        case input_event_type::pointer_cancelled:
        {
            _pointer_known = false;
            set_hover({});
            cancel_capture();
            return;
        }
        default:
        {
            break;
        }
        }
        _pointer_known = true;
        _pointer_x = event.x;
        _pointer_y = event.y;
        const bool in_viewport = event.x >= 0 && event.y >= 0 && event.x < _viewport.logical_width() && event.y < _viewport.logical_height();
        auto       target = in_viewport ? hit(event.x, event.y) : nullptr;
        set_hover(target);
        if(event.type == input_event_type::wheel)
        {
            route_event(target, event);
            return;
        }
        if(event.type == input_event_type::pointer_moved)
        {
            auto active = _capture.lock();
            if(active)
            {
                const auto pressed = active == target;
                if(active->_pressed != pressed)
                {
                    active->_pressed = pressed;
                    _context->paint_dirty = true;
                }
            }
            else
            {
                route_motion(target, event);
                return;
            }
            if(active)
            {
                active->on_event(event);
            }
            return;
        }
        if(event.type == input_event_type::pointer_pressed)
        {
            // One gesture at a time; extra presses cannot steal its capture.
            if(!_capture.expired())
            {
                return;
            }
            if(target)
            {
                _capture = target;
                _capture_button = event.button;
                target->_pressed = true;
                _context->paint_dirty = true;
                if(event.button == mouse_button::primary && focus_eligible(target))
                {
                    set_focus(target);
                }
                // Focus callbacks may disable the target and cancel the gesture.
                if(_capture.lock() == target)
                {
                    target->on_event(event);
                }
            }
        }
        else if(event.type == input_event_type::pointer_released)
        {
            if(event.button != _capture_button)
            {
                return;
            }
            auto captured = _capture.lock();
            // User code may mutate the tree or throw; capture is already ended.
            _capture.reset();
            if(captured)
            {
                captured->_pressed = false;
                _context->paint_dirty = true;
                captured->on_event(event);
            }
        }
    }
};
} // namespace centui

export namespace centui
{
void widget::interaction_changed()
{
    if(auto context = _document.lock())
    {
        context->owner.interaction_changed();
    }
}

void widget::append(const handle<>& child)
{
    append(std::span{std::addressof(child), std::size_t{1}});
}

void widget::append(std::span<const handle<>> children)
{
    check_mutation();
    if(auto context = _document.lock())
    {
        context->owner.append(handle<>{shared_from_this()}, children);
    }
    else
    {
        validate_children(children);
        insert_children(children);
    }
}

void widget::remove(const handle<>& child)
{
    check_mutation();
    (void)child.get();
    if(child._value->_parent_claim.lock() != _identity)
    {
        throw std::invalid_argument("Widget is not a child of this parent");
    }
    if(auto context = _document.lock())
    {
        context->owner.remove(child);
    }
    else
    {
        std::erase(_children, child._value);
        child._value->_parent.reset();
        child._value->_parent_claim.reset();
    }
}

std::vector<handle<>> widget::children() const
{
    std::vector<handle<>> result;
    result.reserve(_children.size());
    for(const auto& child: _children)
    {
        result.emplace_back(child);
    }
    return result;
}

handle<> widget::parent() const
{
    return handle<>{_parent.lock()};
}

bool widget::request_focus()
{
    check_mutation();
    if(auto context = _document.lock())
    {
        return context->owner.request_focus(handle<>{shared_from_this()});
    }
    return false;
}

bool widget::focus_next(bool reverse)
{
    check_mutation();
    if(auto context = _document.lock())
    {
        return context->owner.advance_focus(handle<>{shared_from_this()}, reverse);
    }

    return false;
}

const host_services& widget::host() const
{
    if(auto context = _document.lock(); context && context->owner._host)
    {
        return *context->owner._host;
    }

    static const host_services empty;
    return empty;
}

text_service& widget::fonts()
{
    if(auto context = _document.lock())
    {
        return context->owner.fonts();
    }

    throw std::logic_error("A detached widget has no document text service");
}

template <typename Platform = centui::platform>
class ui
{
public:
    explicit ui(renderer& backend) : _backend(backend)
    {
    }

    ui(Platform& host, renderer& backend) : _backend(backend), _platform(host)
    {
    }

    void render(document& owner)
    {
        attach(owner);
        const auto  viewport = surface();
        const auto& list = owner.prepare(viewport);
        if(viewport.width > 0 && viewport.height > 0)
        {
            _backend.draw(list);
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
        if(!_platform)
        {
            throw std::logic_error("Native events require a platform");
        }
        if(auto translated = _platform->get().translate(event))
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
        if(_platform)
        {
            owner.set_host_services(_platform->get().services());
        }
    }

    centui::viewport surface() const
    {
        return _platform ? _platform->get().viewport() : _backend.viewport();
    }

    renderer& _backend;
    std::optional<std::reference_wrapper<Platform>> _platform;
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

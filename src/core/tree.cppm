module;

#include <algorithm>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <string_view>
#include <variant>

export module centui.tree;

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
// Framework traversal supplies content bounds and the inherited viewport clip.
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
    draw_list& _output;
    text_service& _fonts;
    rect _bounds;
    rect _clip;
    const centui::style& _style;
    details::visual_state _state;
};

} // namespace centui

export namespace centui
{

class widget;
class document;
template <typename T = widget> class handle;
template <typename T = widget> class weak_handle;

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
    bool layout_dirty = true;
    bool paint_dirty = true;
    bool preparing = false;
    std::function<void()> interaction_changed;

    void check_mutation() const
    {
        if(preparing)
        {
            throw std::logic_error("Cannot mutate widgets during measurement or painting");
        }
    }
};
struct widget_access;
}

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

    void set_style(centui::style replacement)
    {
        check_mutation();
        replacement.validate();
        const auto changes = replacement.changes_from(_style);
        if(!changes.layout && !changes.paint)
        {
            return;
        }
        _style = std::move(replacement);
        if(changes.layout)
        {
            invalidate_layout();
        }
        else
        {
            invalidate_paint();
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
            _focused = false;
        }
        invalidate_paint();
        if(auto context = _document.lock())
        {
            if(context->interaction_changed)
            {
                context->interaction_changed();
            }
        }
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

    virtual void on_event(const input_event&)
    {
    }

    void set_interactive(bool value = true)
    {
        check_mutation();
        _interactive = value;
        if(auto context = _document.lock())
        {
            if(context->interaction_changed)
            {
                context->interaction_changed();
            }
        }
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

    // Construction only. make_widget binds weak parent links after construction.
    // A claim token also rejects duplicate ownership inside composite constructors.
    void attach_children(std::span<const handle<>> children);

private:
    friend class document;
    friend struct details::widget_access;
    centui::style _style;
    rect _bounds{};
    rect _content{};
    std::vector<std::shared_ptr<widget>> _children;
    std::weak_ptr<widget> _parent;
    std::shared_ptr<const int> _identity = std::make_shared<const int>(0);
    std::weak_ptr<const int> _parent_claim;
    std::weak_ptr<details::document_context> _document;
    bool _interactive = false;
    bool _enabled = true;
    bool _hovered = false;
    bool _pressed = false;
    bool _focused = false;
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
    template <typename U> requires std::derived_from<U, T>
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
    template <typename> friend class handle;
    template <typename> friend class weak_handle;
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
}

template <typename T, typename... Args>
    requires std::derived_from<T, widget> && (!std::is_abstract_v<T>) && std::constructible_from<T, Args...>
[[nodiscard]] handle<T> make_widget(Args&&... args)
{
    auto value = std::make_shared<T>(std::forward<Args>(args)...);
    details::widget_access::finish(value);
    return handle<T>{std::move(value)};
}

void widget::attach_children(std::span<const handle<>> children)
{
    check_mutation();
    if(!_document.expired())
    {
        throw std::logic_error("Mounted children must be edited through their document");
    }
    std::vector<std::shared_ptr<widget>> batch;
    batch.reserve(children.size());
    for(const auto& child: children)
    {
        (void)child.get();
        auto value = child._value;
        if(value.get() == this || !value->_parent_claim.expired() || !value->_document.expired() ||
           std::ranges::find(batch, value) != batch.end())
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
        batch.push_back(std::move(value));
    }
    _children.reserve(_children.size() + batch.size());
    for(auto& child: batch)
    {
        child->_parent_claim = _identity;
        child->_parent = weak_from_this();
        _children.push_back(std::move(child));
    }
}

} // namespace centui

export namespace centui
{
class document
{
public:
    document() : _context(std::make_shared<details::document_context>())
    {
        _context->interaction_changed = [this]
        {
            synchronize_input();
        };
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
    [[nodiscard]] std::vector<handle<>> children(const handle<>& parent) const
    {
        auto value = checked(parent);
        std::vector<handle<>> result;
        result.reserve(value->_children.size());
        for(const auto& child: value->_children)
        {
            result.emplace_back(child);
        }
        return result;
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
            target->_children.reserve(target->_children.size() + batch.size());
            for(const auto& child: batch)
            {
                child._value->_parent = target;
                child._value->_parent_claim = target->_identity;
                associate(child._value, _context);
                target->_children.push_back(child._value);
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
        update_layout(viewport);
        synchronize_input();
        if(viewport.width <= 0 || viewport.height <= 0)
        {
            _draw.commands.clear();
            _draw.viewport = viewport;
            return _draw;
        }
        if(_context->paint_dirty)
        {
            phase_guard guard{*_context};
            draw_list candidate{viewport, {}};
            if(_root.valid())
            {
                paint_tree(*_root._value, candidate, {0, 0, static_cast<float>(viewport.width), static_cast<float>(viewport.height)}, true);
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
        _context->check_mutation();
        if(_dispatching)
        {
            throw std::logic_error("Recursive event dispatch is not allowed");
        }
        _dispatching = true;
        try
        {
            settle_input(viewport);
            for(const auto& event: events)
            {
                dispatch(event);
                settle_input(viewport);
            }
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
                }
                while(!_edits.empty());
            }
            catch(...)
            {
                // Callback exceptions retain priority over failures applying edits.
            }
            _dispatching = false;
            std::rethrow_exception(failure);
        }
    }

private:
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
        std::shared_ptr<widget> value;
        centui::layout native;
        std::vector<std::unique_ptr<layout_entry>> children;
    };
    handle<> _root;
    std::shared_ptr<details::document_context> _context;
    text_service _fonts;
    std::shared_ptr<const void> _font_revision;
    draw_list _draw;
    centui::viewport _viewport{};
    std::weak_ptr<widget> _hover;
    std::weak_ptr<widget> _capture;
    std::weak_ptr<widget> _focus;
    std::vector<std::function<void()>> _edits;
    bool _dispatching = false;
    bool _host_focused = true;

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
        auto target = checked(parent);
        std::vector<const widget*> seen;
        seen.reserve(children.size());
        for(const auto& child: children)
        {
            auto* value = std::addressof(child.get());
            if(!value->_parent_claim.expired() || !value->_document.expired() ||
               std::ranges::find(seen, value) != seen.end())
            {
                throw std::invalid_argument("Children must be distinct, detached and unmounted");
            }
            for(auto ancestor = target; ancestor; ancestor = ancestor->_parent.lock())
            {
                if(ancestor.get() == value)
                {
                    throw std::invalid_argument("Child insertion would create a cycle");
                }
            }
            seen.push_back(value);
        }
    }
    void dirty_tree() noexcept
    {
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
            operation();
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
        }
        while(!_edits.empty());
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
            store_bounds(*child, box.x, box.y);
        }
    }
    void update_layout(centui::viewport viewport)
    {
        if(_font_revision != _fonts.identity())
        {
            dirty_tree();
        }
        if(viewport.width != _viewport.width || viewport.height != _viewport.height)
        {
            _viewport = viewport;
            dirty_tree();
        }
        if(!_context->layout_dirty || viewport.width <= 0 || viewport.height <= 0)
        {
            return;
        }
        phase_guard guard{*_context};
        if(_root.valid())
        {
            auto tree = make_layout(_root._value);
            tree->native.calculate(static_cast<float>(viewport.width), static_cast<float>(viewport.height));
            check_measurements(*tree);
            store_bounds(*tree, 0, 0);
        }
        _context->layout_dirty = false;
        _context->paint_dirty = true;
        _font_revision = _fonts.identity();
    }
    void paint_tree(const widget& value, draw_list& output, rect clip, bool enabled)
    {
        enabled = enabled && value._enabled;
        const auto state = !enabled ? details::visual_state::disabled :
            value._pressed ? details::visual_state::pressed : value._hovered ? details::visual_state::hovered :
            value._focused ? details::visual_state::focused : details::visual_state::normal;
        painter background{output, _fonts, value._bounds, clip, value._style, state};
        background.rectangle(value._bounds, details::state_color(value._style.background_color, state));
        painter content{output, _fonts, value._content, intersection(clip, value._content), value._style, state};
        value.paint(content);
        for(const auto& child: value._children)
        {
            paint_tree(*child, output, clip, enabled);
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
        for(auto current = value; current; current = current->_parent.lock())
        {
            if(!current->_enabled)
            {
                return false;
            }
        }
        return true;
    }
    std::shared_ptr<widget> hit(const std::shared_ptr<widget>& value, float x, float y) const
    {
        if(!value || !value->_enabled)
        {
            return {};
        }
        for(auto child = value->_children.rbegin(); child != value->_children.rend(); ++child)
        {
            if(auto target = hit(*child, x, y))
            {
                return target;
            }
        }
        return eligible(value) && contains(value->_bounds, x, y) ? value : nullptr;
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
            if(_dispatching)
            {
                captured->on_event({.type = input_event_type::pointer_cancelled});
            }
        }
    }
    void clear_input()
    {
        set_hover({});
        if(auto focused = _focus.lock())
        {
            focused->_focused = false;
            _context->paint_dirty = true;
        }
        _focus.reset();
        cancel_capture();
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
        if(auto focused = _focus.lock(); focused && !eligible(focused))
        {
            focused->_focused = false;
            _focus.reset();
            _context->paint_dirty = true;
        }
    }
    void dispatch(const input_event& event)
    {
        switch(event.type)
        {
        case input_event_type::focus_lost:
        {
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
            set_hover({});
            cancel_capture();
            return;
        }
        default:
        {
            break;
        }
        }
        const bool in_viewport = event.x >= 0 && event.y >= 0 && event.x < _viewport.width && event.y < _viewport.height;
        auto target = in_viewport ? hit(_root._value, event.x, event.y) : nullptr;
        set_hover(target);
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
                active = target;
            }
            if(active)
            {
                active->on_event(event);
            }
            return;
        }
        if(event.button != mouse_button::primary)
        {
            return;
        }
        if(event.type == input_event_type::pointer_pressed)
        {
            cancel_capture();
            if(target)
            {
                if(auto focused = _focus.lock())
                {
                    focused->_focused = false;
                }
                _focus = target;
                _capture = target;
                target->_focused = true;
                target->_pressed = true;
                _context->paint_dirty = true;
                target->on_event(event);
            }
        }
        else if(event.type == input_event_type::pointer_released)
        {
            auto captured = _capture.lock();
            _capture.reset();
            if(captured)
            {
                captured->_pressed = false;
                _context->paint_dirty = true;
                if(captured == target && eligible(captured))
                {
                    captured->on_event(event);
                }
                else
                {
                    captured->on_event({.type = input_event_type::pointer_cancelled});
                }
            }
        }
    }
};
}

export namespace centui
{
class ui
{
public:
    explicit ui(renderer& backend) : _backend(backend)
    {
    }
    void render(document& owner)
    {
        const auto viewport = _backend.viewport();
        const auto& list = owner.prepare(viewport);
        if(viewport.width > 0 && viewport.height > 0)
        {
            _backend.draw(list);
        }
    }
    void process_event(document& owner, const input_event& event)
    {
        owner.process_event(event, _backend.viewport());
    }
    void process_events(document& owner, std::span<const input_event> events)
    {
        owner.process_events(events, _backend.viewport());
    }
private:
    renderer& _backend;
};
}

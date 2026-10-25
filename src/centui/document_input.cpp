#include "document_state.hpp"

namespace centui
{

void document::process_event(const input_event& event, centui::viewport viewport)
{
    process_events(std::span{std::addressof(event), std::size_t{1}}, viewport);
}

void document::process_events(std::span<const input_event> events, centui::viewport viewport)
{
    dispatch_guarded(
        [&]
        {
            settle_input(viewport);

            for(const auto& event: events)
            {
                dispatch(event);
                settle_input(viewport);
            }
        }
    );
}

void document::interaction_changed()
{
    m_data->input.hit_dirty = true;

    if(m_data->dispatching)
    {
        synchronize_input();
    }
    else
    {
        dispatch_guarded(
            [this]
            {
                synchronize_input();
            }
        );
    }
}

bool document::request_focus(const handle<>& target)
{
    m_data->context->check_mutation();

    if(!focus_eligible(checked(target)))
    {
        return false;
    }

    edit(
        [this, target]
        {
            if(focus_eligible(target.m_value))
            {
                set_focus(target.m_value);
            }
        }
    );

    return true;
}

bool document::advance_focus(const handle<>& scope, bool reverse)
{
    std::vector<handle<>> candidates;
    collect_focus(checked(scope), candidates);

    if(candidates.empty())
    {
        return false;
    }

    const auto found = std::ranges::find(candidates, handle<>{m_data->input.focus.lock()});
    std::size_t index = reverse ? candidates.size() - 1 : 0;

    if(found != candidates.end())
    {
        index = static_cast<std::size_t>(found - candidates.begin());
        index = reverse ? (index + candidates.size() - 1) % candidates.size() : (index + 1) % candidates.size();
    }

    return request_focus(candidates[index]);
}

void document::collect_focus(const std::shared_ptr<widget>& value, std::vector<handle<>>& result) const
{
    if(!value || !value->effectively_enabled() || !value->effectively_visible())
    {
        return;
    }

    if(focus_eligible(value))
    {
        result.emplace_back(value);
    }

    for(const auto& child: value->m_children)
    {
        collect_focus(child, result);
    }
}

void document::sync_host()
{
    const auto focused = m_data->input.focus.lock();
    const auto area = focused ? focused->text_input_area() : std::nullopt;

    if(area != m_data->input.text_input_area)
    {
        m_data->input.text_input_area = area;

        if(m_data->input.host && m_data->input.host->text_input)
        {
            m_data->input.host->text_input(area);
        }
    }
}

bool document::contains(rect box, float x, float y) noexcept
{
    return x >= box.x && y >= box.y && x < box.x + box.width && y < box.y + box.height;
}

bool document::eligible(const std::shared_ptr<widget>& value) const
{
    if(!value || !m_data->input.host_focused || m_data->viewport.width <= 0 || m_data->viewport.height <= 0 ||
       value->m_document.lock() != m_data->context || !value->m_interactive ||
       value->m_style.display == display::contents)
    {
        return false;
    }

    return value->effectively_enabled() && value->effectively_visible();
}

bool document::focus_eligible(const std::shared_ptr<widget>& value) const
{
    return value && m_data->input.host_focused && value->m_document.lock() == m_data->context && value->m_focusable &&
           value->m_style.display != display::contents && value->effectively_enabled() && value->effectively_visible();
}

void document::set_focus(std::shared_ptr<widget> value)
{
    auto old = m_data->input.focus.lock();

    if(old == value)
    {
        return;
    }

    m_data->input.focus.reset();
    m_data->context->paint_dirty = true;
    m_data->clear_commands();

    if(old)
    {
        old->m_focused = false;
        old->on_focus_changed(false);
    }

    if(focus_eligible(value))
    {
        m_data->input.focus = value;
        value->m_focused = true;
        value->on_focus_changed(true);
    }
}

std::shared_ptr<widget> document::hit(const std::shared_ptr<widget>& value, float x, float y, rect clip) const
{
    if(!value || !value->m_enabled || value->m_style.visibility != visibility::visible ||
       value->m_style.display == display::none || !contains(clip, x, y))
    {
        return {};
    }

    const auto child_clip = value->m_style.display != display::contents &&
                                    (value->m_clip_children || value->m_style.overflow != overflow::visible)
                                ? intersection(clip, value->m_content)
                                : clip;

    for(auto child = value->m_children.rbegin(); child != value->m_children.rend(); ++child)
    {
        if(auto target = hit(*child, x, y, child_clip))
        {
            return target;
        }
    }

    return eligible(value) && contains(value->m_bounds, x, y) ? value : nullptr;
}

std::shared_ptr<widget> document::hit(float x, float y) const
{
    return hit(m_data->root.m_value, x, y, {0, 0, m_data->viewport.logical_width(), m_data->viewport.logical_height()});
}

void document::route_motion(std::shared_ptr<widget> target, const input_event& event)
{
    for(auto current = std::move(target); current; current = current->m_parent.lock())
    {
        if(eligible(current) && current->on_event(event))
        {
            return;
        }
    }
}

bool document::route_event(std::shared_ptr<widget> target, const input_event& event)
{
    for(auto current = std::move(target); current; current = current->m_parent.lock())
    {
        if(current->effectively_enabled() && current->effectively_visible() && current->on_event(event))
        {
            return true;
        }
    }

    return false;
}

void document::set_hover(std::shared_ptr<widget> value)
{
    if(auto old = m_data->input.hover.lock(); old != value)
    {
        if(old)
        {
            old->m_hovered = false;
        }

        if(value)
        {
            value->m_hovered = true;
        }

        m_data->input.hover = value;
        m_data->context->paint_dirty = true;
        m_data->clear_commands();
    }
}

void document::cancel_capture()
{
    auto captured = m_data->input.capture.lock();
    m_data->input.capture.reset();

    if(captured)
    {
        captured->m_pressed = false;
        m_data->context->paint_dirty = true;
        m_data->clear_commands();
        captured->on_event({.type = input_event_type::pointer_cancelled, .button = m_data->input.capture_button});
    }
}

void document::clear_input()
{
    set_hover({});
    cancel_capture();
    set_focus({});
}

void document::synchronize_input()
{
    if(!eligible(m_data->input.hover.lock()))
    {
        set_hover({});
    }

    if(!eligible(m_data->input.capture.lock()))
    {
        cancel_capture();
    }

    if(auto focused = m_data->input.focus.lock(); focused && !focus_eligible(focused))
    {
        set_focus({});
    }

    if(m_data->input.hit_dirty && !m_data->context->layout_dirty)
    {
        m_data->input.hit_dirty = false;
        const bool inside = m_data->input.pointer_known && m_data->input.host_focused && m_data->input.pointer_x >= 0 &&
                            m_data->input.pointer_y >= 0 &&
                            m_data->input.pointer_x < m_data->viewport.logical_width() &&
                            m_data->input.pointer_y < m_data->viewport.logical_height();
        auto target = inside ? hit(m_data->input.pointer_x, m_data->input.pointer_y) : nullptr;
        set_hover(target);

        if(auto captured = m_data->input.capture.lock())
        {
            const bool pressed = captured == target;

            if(captured->m_pressed != pressed)
            {
                captured->m_pressed = pressed;
                m_data->context->paint_dirty = true;
                m_data->clear_commands();
            }
        }
    }
}

void document::dispatch(const input_event& event)
{
    if(event.type == input_event_type::key_pressed || event.type == input_event_type::key_released ||
       event.type == input_event_type::text_entered || event.type == input_event_type::composition)
    {
        if(!m_data->input.host_focused)
        {
            return;
        }

        if(!route_event(m_data->input.focus.lock(), event) && event.type == input_event_type::key_pressed &&
           event.code == key::tab)
        {
            auto scope = m_data->root.m_value;

            for(auto current = m_data->input.focus.lock(); current; current = current->m_parent.lock())
            {
                if(current->m_focus_scope)
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

    m_data->input.hit_dirty = true;

    switch(event.type)
    {
    case input_event_type::focus_lost:
    {
        m_data->input.pointer_known = false;
        m_data->input.host_focused = false;
        clear_input();

        return;
    }

    case input_event_type::focus_gained:
    {
        m_data->input.host_focused = true;

        return;
    }

    case input_event_type::pointer_left:
    {
        m_data->input.pointer_known = false;
        set_hover({});

        if(auto captured = m_data->input.capture.lock())
        {
            captured->m_pressed = false;
            m_data->context->paint_dirty = true;
            m_data->clear_commands();
        }

        return;
    }

    case input_event_type::pointer_cancelled:
    {
        m_data->input.pointer_known = false;
        set_hover({});
        cancel_capture();

        return;
    }

    default:
    {
        break;
    }
    }

    m_data->input.pointer_known = true;
    m_data->input.pointer_x = event.x;
    m_data->input.pointer_y = event.y;
    const bool in_viewport = event.x >= 0 && event.y >= 0 && event.x < m_data->viewport.logical_width() &&
                             event.y < m_data->viewport.logical_height();
    auto target = in_viewport ? hit(event.x, event.y) : nullptr;
    set_hover(target);

    if(event.type == input_event_type::wheel)
    {
        route_event(target, event);

        return;
    }

    if(event.type == input_event_type::pointer_moved)
    {
        auto active = m_data->input.capture.lock();

        if(active)
        {
            const auto pressed = active == target;

            if(active->m_pressed != pressed)
            {
                active->m_pressed = pressed;
                m_data->context->paint_dirty = true;
                m_data->clear_commands();
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
        if(!m_data->input.capture.expired())
        {
            return;
        }

        if(target)
        {
            m_data->input.capture = target;
            m_data->input.capture_button = event.button;
            target->m_pressed = true;
            m_data->context->paint_dirty = true;
            m_data->clear_commands();

            if(event.button == mouse_button::primary && focus_eligible(target))
            {
                set_focus(target);
            }
            // Focus callbacks may disable the target and cancel the gesture.
            if(m_data->input.capture.lock() == target)
            {
                target->on_event(event);
            }
        }
    }
    else if(event.type == input_event_type::pointer_released)
    {
        if(event.button != m_data->input.capture_button)
        {
            return;
        }

        auto captured = m_data->input.capture.lock();
        // User code may mutate the tree or throw; capture is already ended.
        m_data->input.capture.reset();

        if(captured)
        {
            captured->m_pressed = false;
            m_data->context->paint_dirty = true;
            m_data->clear_commands();
            captured->on_event(event);
        }
    }
}

} // namespace centui

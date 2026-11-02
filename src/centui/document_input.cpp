#include "document_state.hpp"

namespace centui
{

void document::for_each_pointer(const std::function<void()>& operation)
{
    struct restore
    {
        pointer_id& slot;
        pointer_id previous;

        ~restore()
        {
            slot = previous;
        }
    } guard{m_data->input.active_pointer, m_data->input.active_pointer};

    for(auto& [id, pointer]: m_data->input.pointers)
    {
        m_data->input.active_pointer = id;
        operation();
    }
}

void document::track_service(request_id request, const std::shared_ptr<widget>& target)
{
    std::erase_if(
        m_data->input.requests,
        [](const auto& entry)
        {
            return entry.second.expired();
        }
    );
    if(request != 0)
    {
        m_data->input.requests[request] = target;
    }
}

void document::process_service_result(const service_result& result)
{
    dispatch_guarded(
        [&]
        {
            const auto found = m_data->input.requests.find(result.request);
            if(found == m_data->input.requests.end())
            {
                return;
            }
            const auto target = found->second.lock();
            m_data->input.requests.erase(found);
            if(eligible(target))
            {
                target->on_service_result(result);
            }
        }
    );
}

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
                struct input_scope
                {
                    centui::event::input_reference& slot;
                    centui::event::input_reference previous;
                    ~input_scope()
                    {
                        slot = previous;
                    }
                } scope{m_data->active_input, m_data->active_input};
                m_data->active_input = std::cref(event);
                dispatch(event);
                settle_input(viewport);
                if(event.device == pointer_device::touch && (event.type == input_event_type::pointer_released ||
                                                             event.type == input_event_type::pointer_cancelled))
                {
                    m_data->input.pointers.erase(event.pointer);
                    m_data->input.active_pointer = 0;
                }
            }
        }
    );
}

void document::interaction_changed()
{
    details::invalidation_access::invalidate_document(*m_data->context, details::change::interaction);

    if(!m_data->dispatching)
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
    return m_data->focus->request(target.valid() ? target.operator->() : nullptr);
}

handle<> document::focused_widget() const
{
    return handle<>{m_data->focus->current()};
}

bool document::focus_next(bool reverse)
{
    m_data->context->check_mutation();
    return m_data->focus->next(reverse);
}

void document::clear_focus()
{
    m_data->context->check_mutation();
    m_data->focus->clear();
}

void document::enter_modal(const handle<>& root, const handle<>& initial)
{
    m_data->context->check_mutation();
    m_data->focus->enter(root.valid() ? root.operator->() : nullptr, initial.valid() ? initial.operator->() : nullptr);
}

void document::leave_modal(const handle<>& root)
{
    m_data->context->check_mutation();
    m_data->focus->leave(root.valid() ? root.operator->() : nullptr);
}

void document::sync_host()
{
    const auto focused = m_data->focus->current();
    const bool active = focused && focused->focus_active();
    if(active && (m_data->viewport.width <= 0 || m_data->viewport.height <= 0 || m_data->context->layout_dirty ||
                  m_data->context->placement_dirty || m_data->context->geometry_dirty))
    {
        return;
    }
    const auto area = active ? focused->text_input_area() : std::nullopt;
    const auto session =
        area ? std::optional{text_input_session{*area, focused->multiline_text_input()}} : std::nullopt;

    if(session != m_data->input.text_session)
    {
        m_data->input.text_session = session;

        if(m_data->input.host && m_data->input.host->set_text_input)
        {
            m_data->input.host->set_text_input(session);
        }
    }
}

bool document::eligible(const std::shared_ptr<widget>& value) const
{
    if(!value || !m_data->focus->active() || m_data->viewport.width <= 0 || m_data->viewport.height <= 0 ||
       value->m_document.lock() != m_data->context || !m_data->focus->allowed(value) || !value->m_interactive ||
       value->m_style.display == display::contents)
    {
        return false;
    }

    return value->effectively_enabled() && value->effectively_visible();
}

bool document::focus_eligible(const std::shared_ptr<widget>& value) const
{
    return m_data->focus->eligible(value);
}

std::shared_ptr<widget> document::hit(const std::shared_ptr<widget>& value, float x, float y, rect clip) const
{
    if(!value || !value->m_enabled || value->m_style.visibility != visibility::visible ||
       value->m_style.display == display::none || !clip.contains({x, y}))
    {
        return {};
    }

    const auto child_clip = value->m_style.display != display::contents &&
                                    (value->m_clip_children || value->m_style.overflow != overflow::visible)
                                ? intersection(clip, value->m_content)
                                : clip;

    if(eligible(value) && value->m_content.contains({x, y}) &&
       value->hit_test_overlay({x - value->m_content.x, y - value->m_content.y}))
    {
        return value;
    }

    for(auto child = value->m_children.rbegin(); child != value->m_children.rend(); ++child)
    {
        if(auto target = hit(*child, x, y, child_clip))
        {
            return target;
        }
    }

    return eligible(value) && value->m_bounds.contains({x, y}) ? value : nullptr;
}

std::shared_ptr<widget> document::hit(float x, float y) const
{
    return hit(m_data->root.m_value, x, y, {0, 0, m_data->viewport.logical_width(), m_data->viewport.logical_height()});
}

void document::route_motion(std::shared_ptr<widget> target, const input_event& event)
{
    for(auto current = std::move(target); current && m_data->focus->allowed(current);
        current = current->m_parent.lock())
    {
        if(eligible(current) && current->on_event(event))
        {
            return;
        }
    }
}

bool document::route_event(std::shared_ptr<widget> target, const input_event& event)
{
    for(auto current = std::move(target); current && m_data->focus->allowed(current);
        current = current->m_parent.lock())
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
    if(!m_data->input.pointer().hover_capable)
    {
        value.reset();
    }
    if(auto old = m_data->input.pointer().hover.lock(); old != value)
    {
        m_data->input.pointer().hover = value;
        if(old)
        {
            old->m_hovered = std::ranges::any_of(
                m_data->input.pointers,
                [&](const auto& entry)
                {
                    return entry.second.hover.lock() == old;
                }
            );
            old->invalidate_paint();
        }

        if(value)
        {
            value->m_hovered = true;
            value->invalidate_paint();
        }
    }
}

void document::cancel_capture()
{
    auto captured = m_data->input.pointer().capture.lock();
    m_data->input.pointer().capture.reset();
    m_data->input.pointer().scrolling = false;

    if(captured)
    {
        captured->m_pressed = false;
        captured->invalidate_paint();
        captured->on_event(
            {.type = input_event_type::pointer_cancelled,
             .button = m_data->input.pointer().capture_button,
             .pointer = m_data->input.active_pointer,
             .device = m_data->input.pointer().device,
             .hover_capable = m_data->input.pointer().hover_capable}
        );
    }
}

void document::cancel_captures()
{
    std::exception_ptr failure;
    for_each_pointer(
        [&]
        {
            m_data->input.pointer().pointer_known = false;
            set_hover({});
            try
            {
                cancel_capture();
            }
            catch(...)
            {
                if(!failure)
                {
                    failure = std::current_exception();
                }
            }
        }
    );
    if(failure)
    {
        std::rethrow_exception(failure);
    }
}

void document::clear_input()
{
    cancel_captures();
    m_data->focus->clear();
}

void document::synchronize_input()
{
    m_data->focus->reconcile();
    const bool refresh = m_data->input.hit_dirty;
    m_data->input.hit_dirty = false;
    for_each_pointer(
        [&]
        {
            if(!eligible(m_data->input.pointer().hover.lock()))
            {
                set_hover({});
            }

            if(!eligible(m_data->input.pointer().capture.lock()))
            {
                cancel_capture();
            }

            if(refresh && !m_data->context->layout_dirty && !m_data->context->placement_dirty &&
               !m_data->context->geometry_dirty)
            {
                const bool inside = m_data->input.pointer().pointer_known && m_data->focus->active() &&
                                    m_data->input.pointer().pointer_x >= 0 && m_data->input.pointer().pointer_y >= 0 &&
                                    m_data->input.pointer().pointer_x < m_data->viewport.logical_width() &&
                                    m_data->input.pointer().pointer_y < m_data->viewport.logical_height();
                auto target =
                    inside ? hit(m_data->input.pointer().pointer_x, m_data->input.pointer().pointer_y) : nullptr;
                set_hover(target);

                if(auto captured = m_data->input.pointer().capture.lock())
                {
                    const bool pressed = captured == target;

                    if(captured->m_pressed != pressed)
                    {
                        captured->m_pressed = pressed;
                        captured->invalidate_paint();
                    }
                }
            }
        }
    );
    std::erase_if(
        m_data->input.requests,
        [&](const auto& entry)
        {
            return !eligible(entry.second.lock());
        }
    );
}

void document::dispatch(const input_event& event)
{
    if(event.type == input_event_type::key_pressed || event.type == input_event_type::key_released ||
       event.type == input_event_type::text_entered || event.type == input_event_type::composition)
    {
        if(!m_data->focus->active())
        {
            return;
        }

        if(event.type == input_event_type::key_pressed || event.type == input_event_type::text_entered)
        {
            m_data->focus->set_show_focus_indication(true);
        }

        if(!route_event(m_data->focus->current(), event) && event.type == input_event_type::key_pressed &&
           event.code == key::tab)
        {
            focus_next(event.mods.shift);
        }

        return;
    }

    if(event.type != input_event_type::focus_lost && event.type != input_event_type::focus_gained)
    {
        m_data->input.active_pointer = event.pointer;
        auto& pointer = m_data->input.pointer();
        pointer.device = event.device;
        pointer.hover_capable = event.hover_capable && event.device != pointer_device::touch;
    }

    m_data->input.hit_dirty = true;

    switch(event.type)
    {
    case input_event_type::focus_lost:
    {
        m_data->input.pointer().pointer_known = false;
        set_hover({});
        m_data->focus->activate(false);
        cancel_captures();
        sync_host();

        return;
    }

    case input_event_type::focus_gained:
    {
        m_data->focus->activate(true);

        return;
    }

    case input_event_type::pointer_left:
    {
        m_data->input.pointer().pointer_known = false;
        set_hover({});

        if(auto captured = m_data->input.pointer().capture.lock())
        {
            captured->m_pressed = false;
            captured->invalidate_paint();
        }

        return;
    }

    case input_event_type::pointer_cancelled:
    {
        m_data->input.pointer().pointer_known = false;
        set_hover({});
        cancel_capture();

        return;
    }

    default:
    {
        break;
    }
    }

    if(event.type == input_event_type::pointer_pressed && m_data->focus->active())
    {
        m_data->focus->set_show_focus_indication(false);
    }

    m_data->input.pointer().pointer_known = true;
    m_data->input.pointer().pointer_x = event.x;
    m_data->input.pointer().pointer_y = event.y;
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
        auto active = m_data->input.pointer().capture.lock();

        if(active && event.device == pointer_device::touch && !m_data->input.pointer().overlay_gesture)
        {
            auto& pointer = m_data->input.pointer();
            const auto origin = pointer.scrolling ? pointer.previous : pointer.down;
            const vec2 delta{origin.x - event.x, origin.y - event.y};
            const auto dx = event.x - pointer.down.x;
            const auto dy = event.y - pointer.down.y;
            pointer.previous = {event.x, event.y};
            if(pointer.scrolling || dx * dx + dy * dy >= 36.0f)
            {
                auto scroll = event;
                scroll.type = input_event_type::wheel;
                scroll.delta_x = delta.x;
                scroll.delta_y = delta.y;
                if(pointer.scrolling)
                {
                    route_event(active, scroll);
                    return;
                }
                for(auto candidate = active; candidate && m_data->focus->allowed(candidate);
                    candidate = candidate->m_parent.lock())
                {
                    if(eligible(candidate) && candidate->can_scroll(delta))
                    {
                        cancel_capture();
                        if(eligible(candidate))
                        {
                            pointer.capture = candidate;
                            pointer.scrolling = true;
                            candidate->on_event(scroll);
                        }
                        return;
                    }
                }
            }
        }

        if(active)
        {
            const auto pressed = active == target;

            if(active->m_pressed != pressed)
            {
                active->m_pressed = pressed;
                active->invalidate_paint();
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
        // Each pointer keeps its capture; a widget accepts one gesture at a time.
        if(!m_data->input.pointer().capture.expired())
        {
            return;
        }

        if(target)
        {
            if(std::ranges::any_of(
                   m_data->input.pointers,
                   [&](const auto& entry)
                   {
                       return entry.second.capture.lock() == target;
                   }
               ))
            {
                return;
            }
            m_data->input.pointer().capture = target;
            m_data->input.pointer().capture_button = event.button;
            m_data->input.pointer().down = {event.x, event.y};
            m_data->input.pointer().previous = {event.x, event.y};
            m_data->input.pointer().overlay_gesture =
                target->hit_test_overlay({event.x - target->m_content.x, event.y - target->m_content.y});
            target->m_pressed = true;
            target->invalidate_paint();

            if(event.button == mouse_button::primary && focus_eligible(target))
            {
                request_focus(handle<>{target});
                flush_edits();
                synchronize_input();
            }
            // Focus callbacks may disable the target and cancel the gesture.
            if(m_data->input.pointer().capture.lock() == target)
            {
                target->on_event(event);
            }
        }
    }
    else if(event.type == input_event_type::pointer_released)
    {
        if(event.button != m_data->input.pointer().capture_button)
        {
            return;
        }

        auto captured = m_data->input.pointer().capture.lock();
        // User code may mutate the tree or throw; capture is already ended.
        m_data->input.pointer().capture.reset();
        m_data->input.pointer().scrolling = false;

        if(captured)
        {
            captured->m_pressed = false;
            captured->invalidate_paint();
            auto release = event;
            release.target_inside = captured == target;
            captured->on_event(release);
        }
    }
}

} // namespace centui

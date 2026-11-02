#include <atomic>
#include <mutex>
#include <type_traits>
#include <utility>

#include <centui/platform.hpp>

namespace centui
{

platform_event make_platform_event(const input_event& event)
{
    switch(event.type)
    {
    case input_event_type::key_pressed:
    case input_event_type::key_released:
        return key_event{event.code, event.type == input_event_type::key_pressed, event.mods, event.repeat};
    case input_event_type::text_entered:
        return text_event{event.text};
    case input_event_type::composition:
        return composition_event{event.text, event.selection_start, event.selection_length};
    case input_event_type::focus_lost:
    case input_event_type::focus_gained:
        return focus_event{event.type == input_event_type::focus_gained};
    default:
        return pointer_event{
            event.type,
            event.pointer,
            event.device,
            event.x,
            event.y,
            event.button,
            event.mods,
            event.delta_x,
            event.delta_y,
            event.hover_capable,
            event.timestamp
        };
    }
}

std::optional<input_event> make_input_event(const platform_event& event)
{
    return std::visit(
        [](const auto& value) -> std::optional<input_event>
        {
            using type = std::remove_cvref_t<decltype(value)>;
            if constexpr(std::is_same_v<type, pointer_event>)
            {
                return input_event{
                    .type = value.type,
                    .x = value.x,
                    .y = value.y,
                    .button = value.button,
                    .mods = value.mods,
                    .delta_x = value.delta_x,
                    .delta_y = value.delta_y,
                    .pointer = value.pointer,
                    .device = value.device,
                    .hover_capable = value.hover_capable,
                    .timestamp = value.timestamp
                };
            }
            else if constexpr(std::is_same_v<type, key_event>)
            {
                return input_event{
                    .type = value.pressed ? input_event_type::key_pressed : input_event_type::key_released,
                    .code = value.code,
                    .mods = value.mods,
                    .repeat = value.repeat
                };
            }
            else if constexpr(std::is_same_v<type, text_event>)
            {
                return input_event{.type = input_event_type::text_entered, .text = value.text};
            }
            else if constexpr(std::is_same_v<type, composition_event>)
            {
                return input_event{
                    .type = input_event_type::composition,
                    .text = value.text,
                    .selection_start = value.selection_start,
                    .selection_length = value.selection_length
                };
            }
            else if constexpr(std::is_same_v<type, focus_event>)
            {
                return input_event{
                    .type = value.focused ? input_event_type::focus_gained : input_event_type::focus_lost
                };
            }
            else
            {
                return std::nullopt;
            }
        },
        event
    );
}

struct platform::mailbox
{
    std::mutex mutex;
    std::vector<platform_event> events;
};

namespace
{

std::atomic<request_id> next_request{1};

} // namespace

platform::platform()
    : m_mailbox(std::make_shared<mailbox>())
{
}

platform::~platform() = default;

platform_state platform::state() const
{
    auto result = m_state;
    result.viewport = viewport();
    result.drawable = m_surface_available && result.viewport.width > 0 && result.viewport.height > 0;
    return result;
}

void platform::enqueue(platform_event event)
{
    if(auto* pointer = std::get_if<pointer_event>(&event); pointer && pointer->timestamp.count() == 0)
    {
        pointer->timestamp =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
    }
    std::lock_guard lock{m_mailbox->mutex};
    m_mailbox->events.push_back(std::move(event));
}

void platform::enqueue(const input_event& event)
{
    if(event.type == input_event_type::focus_lost || event.type == input_event_type::focus_gained)
    {
        m_state.focused = event.type == input_event_type::focus_gained;
    }
    enqueue(make_platform_event(event));
}

void platform::refresh_state()
{
    const auto current = state();
    if(m_reported && current != *m_reported)
    {
        enqueue(surface_event{surface_change::metrics, current});
    }
    m_reported = current;
}

std::vector<platform_event> platform::take_events()
{
    std::vector<platform_event> events;
    {
        std::lock_guard lock{m_mailbox->mutex};
        events = std::exchange(m_mailbox->events, {});
    }
    for(const auto& event: events)
    {
        if(const auto* focus = std::get_if<focus_event>(&event))
        {
            m_state.focused = focus->focused;
        }
        else if(const auto* lifecycle = std::get_if<lifecycle_event>(&event))
        {
            m_state.closing = m_state.closing || lifecycle->activity == application_activity::closing;
            m_state.active = !m_state.closing && lifecycle->activity == application_activity::active;
        }
    }
    const auto current = state();
    if(m_reported && current != *m_reported)
    {
        events.push_back(surface_event{surface_change::metrics, current});
    }
    m_reported = current;
    return events;
}

void platform::set_activity(application_activity activity)
{
    m_state.closing = m_state.closing || activity == application_activity::closing;
    m_state.active = !m_state.closing && activity == application_activity::active;
    enqueue(lifecycle_event{activity});
}

void platform::set_initial_focus(bool focused)
{
    m_state.focused = focused;
}

bool platform::surface_available() const noexcept
{
    return m_surface_available;
}

void platform::set_surface_available(bool available)
{
    if(m_surface_available == available)
    {
        return;
    }
    m_surface_available = available;
    if(available)
    {
        ++m_state.surface_generation;
    }
    enqueue(surface_event{available ? surface_change::available : surface_change::unavailable, state()});
}

void platform::invalidate_resources()
{
    ++m_state.surface_generation;
    enqueue(surface_event{surface_change::resources_invalidated, state()});
}

void platform::set_safe_area(safe_area_insets insets)
{
    m_state.safe_area = insets;
    refresh_state();
}

void platform::set_keyboard_occlusion(std::optional<rect> area)
{
    m_state.keyboard_occlusion = area;
    refresh_state();
}

platform::service_request platform::begin_request() const
{
    return make_request(m_mailbox);
}

platform::service_request platform::make_request(std::weak_ptr<mailbox> weak)
{
    const auto id = next_request.fetch_add(1, std::memory_order_relaxed);
    auto completed = std::make_shared<std::atomic<bool>>(false);
    return {
        id,
        [weak, completed, id](service_status status, std::string text)
        {
            if(completed->exchange(true))
            {
                return;
            }
            if(const auto queue = weak.lock())
            {
                std::lock_guard lock{queue->mutex};
                queue->events.push_back(service_result{id, status, std::move(text)});
            }
        }
    };
}

std::shared_ptr<host_services> platform::clipboard_services(
    std::function<std::string()> read,
    std::function<void(std::string_view)> write
) const
{
    auto result = std::make_shared<host_services>();
    const std::weak_ptr<mailbox> weak = m_mailbox;
    // Requests may outlive the adapter through a document's borrowed services.
    const auto request = [weak]
    {
        return make_request(weak);
    };
    result->read_clipboard = [weak, request, read = std::move(read)]
    {
        auto pending = request();
        if(weak.expired())
        {
            return pending.id;
        }
        try
        {
            pending.complete(
                read ? service_status::success : service_status::unsupported,
                read ? read() : std::string{}
            );
        }
        catch(...)
        {
            pending.complete(service_status::failed, {});
        }
        return pending.id;
    };
    result->write_clipboard = [weak, request, write = std::move(write)](std::string text)
    {
        auto pending = request();
        if(weak.expired())
        {
            return pending.id;
        }
        try
        {
            if(write)
            {
                write(text);
            }
            pending.complete(write ? service_status::success : service_status::unsupported, {});
        }
        catch(...)
        {
            pending.complete(service_status::failed, {});
        }
        return pending.id;
    };
    return result;
}

} // namespace centui

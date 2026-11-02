#pragma once

#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>

#include <centui/document.hpp>
#include <centui/platform.hpp>

namespace centui
{

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

    /// \brief Processes polled events, advances widgets using the internal clock, and renders.
    /// Returns false when the backend cannot begin a frame. Does not poll events.
    bool frame(document& owner)
    {
        const frame_duration elapsed = std::chrono::steady_clock::now() - m_previous;

        return frame(owner, elapsed);
    }

    /// \brief Runs one frame with application-supplied elapsed time and resets the internal clock.
    bool frame(document& owner, frame_duration elapsed)
    {
        if(!std::isfinite(elapsed.count()) || elapsed.count() < 0)
        {
            throw std::invalid_argument("Frame duration must be finite and nonnegative");
        }
        m_previous = std::chrono::steady_clock::now();

        if(m_platform)
        {
            attach(owner);
            process_events(owner, m_platform->get().take_events());
        }

        if(m_resumed)
        {
            elapsed = frame_duration::zero();
            m_resumed = false;
        }
        if(m_state.active)
        {
            owner.advance(elapsed);
        }

        if(!m_state.active || !m_state.drawable)
        {
            return false;
        }

        if(!m_backend.begin_frame())
        {
            return false;
        }

        render(owner);
        m_backend.end_frame();

        return true;
    }

    void render(document& owner)
    {
        attach(owner);
        if(!m_state.active || !m_state.drawable)
        {
            return;
        }

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

    void process_event(document& owner, const platform_event& event)
    {
        attach(owner);
        if(const auto* focus = std::get_if<focus_event>(&event))
        {
            m_state.focused = focus->focused;
            synchronize_activity(owner);
        }
        else if(const auto* lifecycle = std::get_if<lifecycle_event>(&event))
        {
            m_state.closing = m_state.closing || lifecycle->activity == application_activity::closing;
            const bool active = !m_state.closing && lifecycle->activity == application_activity::active;
            m_resumed = m_resumed || (!m_state.active && active);
            m_state.active = active;
            synchronize_activity(owner);
        }
        else if(const auto* changed = std::get_if<surface_event>(&event))
        {
            const bool active = m_state.active;
            const bool focused = m_state.focused;
            const bool closing = m_state.closing;
            m_state = changed->state;
            // Surface snapshots cannot override ordered focus/lifecycle notifications.
            m_state.active = active;
            m_state.focused = focused;
            m_state.closing = closing;
            m_backend.on_surface_event({changed->change, m_state});
            synchronize_activity(owner);
        }
        else if(const auto* result = std::get_if<service_result>(&event))
        {
            owner.process_service_result(*result);
        }
        else if(m_state.active && m_state.drawable)
        {
            if(auto input = make_input_event(event))
            {
                owner.process_event(*input, surface());
            }
        }
    }

    template <typename Event>
        requires(
            requires(Platform& host, const Event& event) { host.ingest(event); } ||
            requires(Platform& host, const Event& event) { host.translate(event); }
        )
    void process_event(document& owner, const Event& event)
    {
        if(!m_platform)
        {
            throw std::logic_error("Native events require a platform");
        }

        if constexpr(requires(Platform& host) { host.ingest(event); })
        {
            m_platform->get().ingest(event);
            process_events(owner, m_platform->get().take_events());
        }
        else
        {
            if(auto translated = m_platform->get().translate(event))
            {
                process_event(owner, *translated);
            }
        }
    }

    void process_events(document& owner, std::span<const input_event> events)
    {
        attach(owner);
        owner.process_events(events, surface());
    }

    void process_events(document& owner, std::span<const platform_event> events)
    {
        for(const auto& event: events)
        {
            process_event(owner, event);
        }
    }

private:
    void attach(document& owner)
    {
        if(m_platform)
        {
            if(!m_attached)
            {
                m_state = m_platform->get().state();
                m_attached = true;
                if(!m_state.active || !m_state.focused || !m_state.drawable)
                {
                    synchronize_activity(owner);
                }
            }
            owner.set_host_services(m_platform->get().services());
        }
    }

    void synchronize_activity(document& owner)
    {
        owner.process_event(
            {.type = m_state.active && m_state.focused && m_state.drawable ? input_event_type::focus_gained
                                                                           : input_event_type::focus_lost},
            surface()
        );
    }

    centui::viewport surface() const
    {
        return m_platform ? m_platform->get().viewport() : m_backend.viewport();
    }

    renderer& m_backend;
    std::optional<std::reference_wrapper<Platform>> m_platform;
    std::chrono::steady_clock::time_point m_previous = std::chrono::steady_clock::now();
    platform_state m_state;
    bool m_attached = false;
    bool m_resumed = false;
};

} // namespace centui

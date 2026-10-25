#pragma once

#include <chrono>
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
        m_previous = std::chrono::steady_clock::now();

        if(m_platform)
        {
            process_events(owner, m_platform->get().events());
        }

        owner.advance(elapsed);

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
    std::chrono::steady_clock::time_point m_previous = std::chrono::steady_clock::now();
};

} // namespace centui

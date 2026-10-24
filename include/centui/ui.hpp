#pragma once

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

/// \brief Polls events, runs the optional update, advances widgets, and renders a frame.
template <typename Platform, typename Renderer>
void run(Platform& host, Renderer& backend, document& owner, std::function<void(frame_duration)> update = {})
{
    ui integration{host, backend};
    auto previous = std::chrono::steady_clock::now();

    while(host.poll_events())
    {
        const auto now = std::chrono::steady_clock::now();
        const frame_duration elapsed = now - previous;
        previous = now;
        integration.process_events(owner, host.events());
        if(update)
        {
            update(elapsed);
        }
        owner.advance(elapsed);

        if(backend.begin_frame())
        {
            integration.render(owner);
            backend.end_frame();
        }
    }
}

} // namespace centui

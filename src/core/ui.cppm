module;

#include <cstddef>
#include <memory>
#include <span>

export module centui.ui;

export import centui.element;
export import centui.document;
export import centui.viewport;
export import centui.input;
import centui.runtime;
import centui.dispatcher;

export namespace centui
{

// Backend supplies viewport(), glyphs(), and synchronous draw(commands).
// Persistent interaction state belongs to the document, not this adapter.
template <typename backend_type>
class ui
{
public:
    explicit ui(backend_type& backend) : _backend(backend)
    {
    }

    void render(element& root)
    {
        const auto [width, height] = _backend.viewport();
        if(width > 0 && height > 0)
        {
            _backend.draw(details::prepared_commands(root, _backend.glyphs(), width, height));
        }
    }

    void render(document& owner)
    {
        if(owner.empty())
        {
            const auto [width, height] = _backend.viewport();
            if(width > 0 && height > 0)
            {
                details::document_access::complete_empty(owner, {.topology = true, .text = true, .layout = true, .paint = true});
            }
            return;
        }
        auto root = owner.root();
        render(root);
    }

    void process_event(document& owner, const input_event& event)
    {
        process_events(owner, std::span{std::addressof(event), std::size_t{1}});
    }

    void process_events(document& owner, std::span<const input_event> events)
    {
        details::input_dispatcher dispatcher;
        auto                      initial = details::document_access::state(owner);
        dispatcher.synchronize(initial, initial->root(), _backend.viewport());
        for(const auto& event: events)
        {
            // A callback may mount a new root or replace/move the document.
            auto state = details::document_access::current(owner);
            if(!state)
            {
                break;
            }
            dispatcher.process(state, state->root(), _backend.glyphs(), _backend.viewport(), event);
        }
    }

    void process_event(element& root, const input_event& event)
    {
        process_events(root, std::span{std::addressof(event), std::size_t{1}});
    }

    void process_events(element& root, std::span<const input_event> events)
    {
        details::input_dispatcher dispatcher;
        auto                      initial = details::element_access::state(root);
        const auto                binding = details::element_access::binding(root);
        dispatcher.synchronize(initial, binding->id, _backend.viewport());
        for(const auto& event: events)
        {
            auto state = binding->owner.lock();
            if(!state || !state->contains(binding->id))
            {
                break;
            }
            dispatcher.process(state, binding->id,
                               _backend.glyphs(), _backend.viewport(), event);
        }
    }

private:
    backend_type& _backend;
};

} // namespace centui

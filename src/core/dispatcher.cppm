module;

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

export module centui.dispatcher;

import centui.runtime;
import centui.model;
import centui.input;
import centui.unit;
import centui.viewport;
import centui.glyph;
import centui.preparation;

export namespace centui::details
{

// One synchronous event batch. Runtime guards survive root replacement and
// unwind on exceptions; this object never owns persistent interaction targets.
class input_dispatcher
{
public:
    input_dispatcher() = default;
    input_dispatcher(const input_dispatcher&) = delete;
    input_dispatcher& operator=(const input_dispatcher&) = delete;

    ~input_dispatcher()
    {
        for(const auto& state: _active)
        {
            state->interaction.dispatching = false;
        }
    }

    void synchronize(const std::shared_ptr<document_state>& state, std::optional<node_id> scope, viewport size)
    {
        claim(state);
        state->prune_interaction();
        if(!scope || !state->contains(*scope) || size.width <= 0 || size.height <= 0)
        {
            state->cancel_interaction();
        }
        else
        {
            auto& input = state->interaction;
            if(input.hovered && !state->is_within(*input.hovered, *scope))
            {
                state->set_hover(std::nullopt);
            }
            if(input.focused && !state->is_within(*input.focused, *scope))
            {
                state->set_focus(std::nullopt);
            }
            if(input.captured && !state->is_within(*input.captured, *scope))
            {
                state->set_capture(std::nullopt);
            }
        }

        if(scope && state->contains(*scope) && state->pending().interaction)
        {
            state->complete(*scope, {.interaction = true});
        }
        else if(!state->root())
        {
            state->complete_empty({.interaction = true});
        }
    }

    void process(const std::shared_ptr<document_state>& state, std::optional<node_id> scope,
                 font& face, viewport size, const input_event& event)
    {
        synchronize(state, scope, size);
        if(event.type == input_event_type::focus_lost)
        {
            state->interaction.host_focused = false;
            state->cancel_interaction();
            return;
        }
        if(event.type == input_event_type::focus_gained)
        {
            state->interaction.host_focused = true;
            return;
        }
        if(event.type == input_event_type::pointer_cancelled)
        {
            state->cancel_interaction();
            return;
        }
        if(event.type == input_event_type::pointer_left)
        {
            state->set_hover(std::nullopt);
            return;
        }
        if(!state->interaction.host_focused || !scope || !state->contains(*scope) ||
           size.width <= 0 || size.height <= 0)
        {
            return;
        }

        prepare_geometry(*state, face, size.width, size.height);
        const auto target = hit_test(*state, *scope, event.x, event.y);
        state->set_hover(target);
        if(event.button != mouse_button::primary)
        {
            return;
        }
        if(event.type == input_event_type::pointer_pressed)
        {
            state->set_focus(target);
            state->set_capture(target);
            return;
        }
        if(event.type != input_event_type::pointer_released)
        {
            return;
        }

        const auto captured = state->interaction.captured;
        state->set_capture(std::nullopt);
        if(captured && captured == target && state->eligible(*captured))
        {
            // Keep the callable, not a node reference, across application code.
            // Mutations persist if it throws; capture is already released.
            auto callback = state->get(*captured).content.interaction->activate;
            if(callback)
            {
                (*callback)();
            }
        }
    }

private:
    void claim(const std::shared_ptr<document_state>& state)
    {
        state->check_thread();
        for(const auto& active: _active)
        {
            if(active == state)
            {
                return;
            }
        }
        if(state->interaction.dispatching)
        {
            throw std::logic_error("Recursive input dispatch into the same runtime is not allowed");
        }
        _active.push_back(state);
        state->interaction.dispatching = true;
    }

    static std::optional<node_id> hit_test(const document_state& state, node_id scope, float x, float y)
    {
        for(auto parent = state.get(scope).parent; parent; parent = state.get(*parent).parent)
        {
            const auto& capability = state.get(*parent).content.interaction;
            if(capability && !capability->enabled)
            {
                return std::nullopt;
            }
        }

        const auto absolute = state.absolute_bounds(scope);
        const auto local = state.layout.bounds(scope.index);
        auto       visit = [&](this auto&& self, node_id id, float parent_x, float parent_y) -> std::optional<node_id>
        {
            const auto& record = state.get(id);
            const auto& capability = record.content.interaction;
            if(capability && !capability->enabled)
            {
                return std::nullopt;
            }
            auto bounds = state.layout.bounds(id.index);
            bounds.x += parent_x;
            bounds.y += parent_y;

            // Last-painted eligible descendant wins. Noninteractive content is
            // transparent to input; overflow is visible until clipping exists.
            for(auto child = record.children.rbegin(); child != record.children.rend(); ++child)
            {
                if(auto target = self(*child, bounds.x, bounds.y))
                {
                    return target;
                }
            }
            if(capability && x >= bounds.x && y >= bounds.y &&
               x < bounds.x + bounds.width && y < bounds.y + bounds.height)
            {
                return id;
            }
            return std::nullopt;
        };
        return visit(scope, absolute.x - local.x, absolute.y - local.y);
    }

    std::vector<std::shared_ptr<document_state>> _active;
};

} // namespace centui::details

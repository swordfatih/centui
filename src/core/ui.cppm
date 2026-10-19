module;

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <ranges>
#include <span>
#include <utility>
#include <variant>
#include <vector>

export module centui.ui;

export import centui.element;
export import centui.document;
export import centui.viewport;
export import centui.input;
import centui.node;
import centui.unit;

export namespace centui
{

// Borrows a concrete backend; no graphics or window dependency.
// Backend supplies viewport(), glyphs(), and synchronous draw(commands).
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
        if(width <= 0 || height <= 0)
        {
            return;
        }
        const auto list = prepare(root, _backend.glyphs(), width, height);
        _backend.draw(list);
    }

    void render(document& owner)
    {
        if(owner.empty()) return;
        auto root = owner.root();
        render(root);
    }

    void process_event(document& owner, const input_event& event)
    {
        process_events(owner, std::span{std::addressof(event), std::size_t{1}});
    }

    void process_events(document& owner, std::span<const input_event> events)
    {
        if(owner.empty()) { clear_interaction(); return; }
        auto root = owner.root();
        process_events(root, events);
    }

    void process_event(element& root, const input_event& event)
    {
        process_events(root, std::span{std::addressof(event), std::size_t{1}});
    }

    void process_events(element& root, std::span<const input_event> events)
    {
        if(events.empty())
        {
            return;
        }

        auto& tree = details::element_access::get(root);
        validate_interaction(root);

        const auto [width, height] = _backend.viewport();
        if(width <= 0 || height <= 0)
        {
            clear_interaction();
            return;
        }

        details::attach_elements(root, _backend.glyphs());
        tree.calculate(static_cast<float>(width), static_cast<float>(height));
        std::vector<std::pair<std::weak_ptr<details::button_interaction_state>, std::function<void()>>> callbacks;

        for(const auto& event: events)
        {
            if(event.type == input_event_type::pointer_left)
            {
                set_hover({});
                continue;
            }

            auto target = hit_test(root, 0.0f, 0.0f, event.x, event.y);
            set_hover(target);

            if(event.button != mouse_button::primary)
            {
                continue;
            }

            if(event.type == input_event_type::pointer_pressed)
            {
                set_focus(target);
                set_capture(target);
            }
            else if(event.type == input_event_type::pointer_released)
            {
                auto captured = _captured.lock();
                if(captured)
                {
                    const bool was_pressed = captured->pressed;
                    captured->pressed = false;
                    if(was_pressed && captured->enabled && captured == target && captured->on_click)
                    {
                        callbacks.emplace_back(captured, captured->on_click);
                        _captured.reset();
                        continue;
                    }
                }
                _captured.reset();
            }
        }

        for(auto& [target, callback]: callbacks)
        {
            if(const auto state = target.lock(); state && state->enabled)
            {
                callback();
            }
        }
    }

private:
    using interaction_ptr = std::shared_ptr<details::button_interaction_state>;

    static bool contains(const element& current, const details::button_interaction_state* state)
    {
        const auto& value = details::element_access::get(current);
        if(const auto* button = std::get_if<details::button_data>(&value.data);
           button && button->interaction.get() == state)
        {
            return true;
        }

        return std::ranges::any_of(details::element_access::children(current), [&](const element& child)
        {
            return contains(child, state);
        });
    }

    static interaction_ptr hit_test(const element& current, float parent_x, float parent_y, float x, float y)
    {
        const auto& value = details::element_access::get(current);
        const auto local = value.layout.bounds();
        const auto bounds = rect{local.x + parent_x, local.y + parent_y, local.width, local.height};
        interaction_ptr target;

        if(const auto* button = std::get_if<details::button_data>(&value.data);
           button && button->interaction->enabled &&
           x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height)
        {
            target = button->interaction;
        }

        for(const auto& child: details::element_access::children(current))
        {
            if(const auto child_target = hit_test(child, bounds.x, bounds.y, x, y))
            {
                target = child_target;
            }
        }

        return target;
    }

    void set_hover(const interaction_ptr& target)
    {
        if(auto previous = _hovered.lock(); previous && previous != target)
        {
            previous->hovered = false;
        }
        _hovered = target;
        if(target)
        {
            target->hovered = true;
        }
    }

    void set_focus(const interaction_ptr& target)
    {
        if(auto previous = _focused.lock(); previous && previous != target)
        {
            previous->focused = false;
        }
        _focused = target;
        if(target)
        {
            target->focused = true;
        }
    }

    void set_capture(const interaction_ptr& target)
    {
        if(auto previous = _captured.lock())
        {
            previous->pressed = false;
        }
        _captured = target;
        if(target)
        {
            target->pressed = true;
        }
    }

    void validate_interaction(const element& root)
    {
        if(auto hovered = _hovered.lock(); hovered && (!hovered->enabled || !contains(root, hovered.get())))
        {
            hovered->hovered = false;
            _hovered.reset();
        }
        if(auto captured = _captured.lock(); captured && (!captured->enabled || !contains(root, captured.get())))
        {
            captured->pressed = false;
            _captured.reset();
        }
        if(auto focused = _focused.lock(); focused && (!focused->enabled || !contains(root, focused.get())))
        {
            focused->focused = false;
            _focused.reset();
        }
    }

    void clear_interaction()
    {
        if(auto hovered = _hovered.lock())
        {
            hovered->hovered = false;
        }
        if(auto captured = _captured.lock())
        {
            captured->pressed = false;
        }
        if(auto focused = _focused.lock())
        {
            focused->focused = false;
        }
        _hovered.reset();
        _captured.reset();
        _focused.reset();
    }

    backend_type& _backend;
    std::weak_ptr<details::button_interaction_state> _hovered;
    std::weak_ptr<details::button_interaction_state> _captured;
    std::weak_ptr<details::button_interaction_state> _focused;
};

} // namespace centui

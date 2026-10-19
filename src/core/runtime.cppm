module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

export module centui.runtime;

export import centui.model;
export import centui.diagnostics;
import centui.style;
import centui.unit;
import centui.text_service;
import centui.layout_engine;
import centui.painter;
import centui.primitives;

export namespace centui::details
{

class document_state;

// Only construction handles retain this owner. Composition and mounting
// relinquish it, so surviving handles cannot keep an adopted tree alive.
struct construction_owner
{
    std::shared_ptr<document_state> state;
    ~construction_owner();
};

struct element_binding
{
    std::weak_ptr<document_state>     owner;
    std::weak_ptr<construction_owner> construction;
    node_id                           id;
};

struct document_record
{
    element_content                  content;
    std::shared_ptr<element_binding> binding;
    std::optional<node_id>           parent;
    std::vector<node_id>             children;
    invalidation                     pending;
};

struct document_slot
{
    std::uint64_t                    generation = 1;
    std::unique_ptr<document_record> record;
};

struct geometry_inputs
{
    std::shared_ptr<const void> face;
    int                         width = 0;
    int                         height = 0;
    bool                        valid = false;
};

struct input_state
{
    std::optional<node_id> hovered;
    std::optional<node_id> focused;
    std::optional<node_id> captured;
    bool                   host_focused = true;
    bool                   dispatching = false;
};

class document_state : public std::enable_shared_from_this<document_state>
{
public:
    static constexpr std::size_t end = std::numeric_limits<std::size_t>::max();

    document_state() = default;
    document_state(const document_state&) = delete;
    document_state& operator=(const document_state&) = delete;

    void check_thread() const
    {
        if(_thread != std::this_thread::get_id())
        {
            throw std::logic_error("Tree access requires its creating thread");
        }
    }

    [[nodiscard]] document_record& get(node_id id) const
    {
        check_thread();
        if(_retired || id.index >= _slots.size() || _slots[id.index].generation != id.generation || !_slots[id.index].record)
        {
            throw invalid_element{};
        }
        return *_slots[id.index].record;
    }

    [[nodiscard]] std::optional<node_id> root() const
    {
        check_thread();
        return _root;
    }

    [[nodiscard]] std::size_t slot_count() const noexcept
    {
        return _slots.size();
    }

    [[nodiscard]] invalidation pending() const
    {
        check_thread();
        return _pending;
    }

    std::shared_ptr<element_binding> create_root(element_content content)
    {
        check_thread();
        content.style.validate();
        if(_root || !_slots.empty())
        {
            throw std::logic_error("Construction runtime already contains a tree");
        }

        auto record = std::make_unique<document_record>();
        auto binding = std::make_shared<element_binding>();
        binding->owner = shared_from_this();
        binding->id = {0, 1};
        record->content = std::move(content);
        record->binding = binding;
        text.resize(1);
        layout.resize(1);
        _slots.push_back({1, std::move(record)});
        _root = binding->id;
        invalidate(binding->id, {.topology = true,
                                 .text = !primitive_has_children(get(binding->id).content),
                                 .layout = true,
                                 .paint = true,
                                 .interaction = true});
        return binding;
    }

    [[nodiscard]] std::vector<node_id> preorder(node_id root) const
    {
        std::vector<node_id> result;
        auto                 visit = [&](this auto&& self, node_id current) -> void
        {
            result.push_back(current);
            for(const auto child: get(current).children)
            {
                self(child);
            }
        };
        visit(root);
        return result;
    }

    void invalidate(node_id id, invalidation changes)
    {
        auto& record = get(id);
        record.pending.merge(changes);
        _pending.merge(changes);
        if(changes.paint)
        {
            paint.invalidate();
        }

        const invalidation ancestors{.layout = changes.layout, .paint = changes.paint};
        for(auto parent = record.parent; parent && ancestors.any(); parent = get(*parent).parent)
        {
            get(*parent).pending.merge(ancestors);
        }
        if(changes.interaction)
        {
            prune_interaction();
        }
    }

    void complete(node_id root, invalidation completed)
    {
        auto visit = [&](this auto&& self, node_id current) -> void
        {
            auto& record = get(current);
            record.pending.clear(completed);
            for(const auto child: record.children)
            {
                self(child);
            }
        };
        visit(root);
        if(_root == root)
        {
            _pending.clear(completed);
        }
        else
        {
            _pending = {};
            for(const auto& slot: _slots)
            {
                if(slot.record)
                {
                    _pending.merge(slot.record->pending);
                }
            }
        }
    }

    void complete_empty(invalidation completed)
    {
        check_thread();
        if(!_root)
        {
            _pending.clear(completed);
        }
    }

    // Both composition and insertion transfer complete construction runtimes.
    // Every allocation and ownership check precedes the transfer commit.
    void insert(node_id parent, std::span<const std::shared_ptr<element_binding>> children,
                std::size_t index = end)
    {
        auto&      target = container(parent);
        const auto position = insertion_index(index, target.children.size());

        struct transfer
        {
            std::shared_ptr<document_state> source;
            node_id                         from;
            node_id                         to;
            std::optional<node_id>          parent;
            std::vector<node_id>            children;
        };

        std::vector<std::shared_ptr<document_state>> sources;
        std::vector<transfer>                        plan;
        std::vector<node_id>                         roots;
        for(const auto& binding: children)
        {
            auto source = owner_of(binding);
            if(source.get() == this || source->mounted || source->root() != binding->id ||
               std::ranges::find(sources, source) != sources.end())
            {
                throw std::invalid_argument("Child already has an owner, is duplicated, or creates a cycle");
            }
            sources.push_back(source);

            const auto           nodes = source->preorder(binding->id);
            std::vector<node_id> remap(source->slot_count());
            const auto           begin = plan.size();
            for(const auto from: nodes)
            {
                const auto    offset = plan.size();
                const auto    reused = offset < _free_slots.size();
                const auto    destination = reused ? _free_slots[_free_slots.size() - 1 - offset]
                                                   : _slots.size() + offset - _free_slots.size();
                const node_id to{destination, reused ? _slots[destination].generation : 1};
                remap[from.index] = to;
                plan.push_back({source, from, to, {}, {}});
            }
            roots.push_back(remap[binding->id.index]);
            for(std::size_t i = begin; i < plan.size(); ++i)
            {
                auto&       item = plan[i];
                const auto& record = source->get(item.from);
                item.parent = record.parent ? remap[record.parent->index] : parent;
                item.children.reserve(record.children.size());
                for(const auto child: record.children)
                {
                    item.children.push_back(remap[child.index]);
                }
            }
        }

        if(plan.empty())
        {
            return;
        }
        const auto reused = std::min(plan.size(), _free_slots.size());
        const auto size = _slots.size() + plan.size() - reused;
        target.children.reserve(target.children.size() + roots.size());
        text.resize(size);
        layout.resize(size);
        _slots.resize(size);

        for(auto& item: plan)
        {
            auto record = std::move(item.source->_slots[item.from.index].record);
            record->parent = item.parent;
            record->children.swap(item.children);
            record->binding->owner = weak_from_this();
            record->binding->id = item.to;
            _pending.merge(record->pending);
            _slots[item.to.index].record = std::move(record);
            text.transfer_from(item.source->text, item.from.index, item.to.index);
            layout.transfer_from(item.source->layout, item.from.index, item.to.index);
        }
        _free_slots.resize(_free_slots.size() - reused);
        target.children.insert(target.children.begin() + position, roots.begin(), roots.end());
        for(std::size_t i = 0; i < sources.size(); ++i)
        {
            sources[i]->_root.reset();
            if(auto construction = children[i]->construction.lock())
            {
                construction->state.reset();
            }
        }
        invalidate(parent, {.topology = true, .layout = true, .paint = true, .interaction = true});
    }

    void reparent(node_id child, node_id parent, std::size_t index)
    {
        auto& source = get(child);
        auto& target = container(parent);
        if(!source.parent)
        {
            throw std::invalid_argument("Cannot reparent the document root");
        }
        for(auto ancestor = std::optional{parent}; ancestor; ancestor = get(*ancestor).parent)
        {
            if(*ancestor == child)
            {
                throw std::invalid_argument("Reparenting would create a cycle");
            }
        }

        const auto previous = *source.parent;
        const bool same_parent = previous == parent;
        const auto position = insertion_index(index, target.children.size() - (same_parent ? 1 : 0));
        if(same_parent && target.children[position] == child)
        {
            return;
        }

        target.children.reserve(target.children.size() + (same_parent ? 0 : 1));
        layout.detach_children(previous.index);
        std::erase(get(previous).children, child);
        target.children.insert(target.children.begin() + position, child);
        source.parent = parent;
        const invalidation changes{.topology = true, .layout = true, .paint = true, .interaction = true};
        invalidate(previous, changes);
        invalidate(parent, changes);
    }

    void reorder(node_id parent, std::span<const node_id> order)
    {
        auto& target = container(parent);
        if(order.size() != target.children.size())
        {
            throw std::invalid_argument("Incomplete child order");
        }

        std::vector<node_id> replacement;
        replacement.reserve(order.size());
        for(const auto child: order)
        {
            if(get(child).parent != parent || std::ranges::find(replacement, child) != replacement.end())
            {
                throw std::invalid_argument("Order contains a foreign or duplicate child");
            }
            replacement.push_back(child);
        }
        if(replacement == target.children)
        {
            return;
        }

        target.children.swap(replacement);
        invalidate(parent, {.topology = true, .layout = true, .paint = true, .interaction = true});
    }

    void remove(node_id child)
    {
        auto&      record = get(child);
        const auto removed = preorder(child);
        _free_slots.reserve(_free_slots.size() + removed.size());
        cancel_interaction(child);
        const invalidation changes{.topology = true, .layout = true, .paint = true, .interaction = true};
        if(record.parent)
        {
            const auto parent = *record.parent;
            layout.detach_children(parent.index);
            std::erase(get(parent).children, child);
            invalidate(parent, changes);
        }
        else
        {
            _root.reset();
            _pending.merge(changes);
            geometry.valid = false;
        }

        paint.invalidate();
        for(const auto id: removed)
        {
            layout.detach_children(id.index);
        }
        for(const auto id: removed)
        {
            auto& slot = _slots[id.index];
            layout.erase(id.index);
            text.erase(id.index);
            slot.record.reset();
            if(slot.generation != std::numeric_limits<std::uint64_t>::max())
            {
                ++slot.generation;
                _free_slots.push_back(id.index);
            }
        }
    }

    [[nodiscard]] rect absolute_bounds(node_id id) const
    {
        auto bounds = layout.bounds(id.index);
        for(auto parent = get(id).parent; parent; parent = get(*parent).parent)
        {
            const auto offset = layout.bounds(parent->index);
            bounds.x += offset.x;
            bounds.y += offset.y;
        }
        return bounds;
    }

    [[nodiscard]] bool contains(node_id id) const
    {
        check_thread();
        return !_retired && id.index < _slots.size() &&
               _slots[id.index].generation == id.generation && static_cast<bool>(_slots[id.index].record);
    }

    [[nodiscard]] bool is_within(node_id id, node_id ancestor) const
    {
        if(!contains(id) || !contains(ancestor))
        {
            return false;
        }
        for(auto current = std::optional{id}; current; current = get(*current).parent)
        {
            if(*current == ancestor)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool eligible(node_id id) const
    {
        if(!contains(id) || !get(id).content.interaction)
        {
            return false;
        }
        for(auto current = std::optional{id}; current; current = get(*current).parent)
        {
            const auto& capability = get(*current).content.interaction;
            if(capability && !capability->enabled)
            {
                return false;
            }
        }
        return true;
    }

    void set_hover(std::optional<node_id> target)
    {
        set_target(interaction.hovered, target);
    }

    void set_focus(std::optional<node_id> target)
    {
        set_target(interaction.focused, target);
    }

    void set_capture(std::optional<node_id> target)
    {
        set_target(interaction.captured, target);
    }

    void cancel_interaction(std::optional<node_id> subtree = std::nullopt)
    {
        const auto affected = [&](std::optional<node_id> target)
        {
            return target && (!subtree || is_within(*target, *subtree));
        };
        if(affected(interaction.hovered))
        {
            set_hover(std::nullopt);
        }
        if(affected(interaction.focused))
        {
            set_focus(std::nullopt);
        }
        if(affected(interaction.captured))
        {
            set_capture(std::nullopt);
        }
    }

    void prune_interaction()
    {
        if(interaction.hovered && !eligible(*interaction.hovered))
        {
            set_hover(std::nullopt);
        }
        if(interaction.focused && !eligible(*interaction.focused))
        {
            set_focus(std::nullopt);
        }
        if(interaction.captured && !eligible(*interaction.captured))
        {
            set_capture(std::nullopt);
        }
    }

    void set_enabled(node_id id, bool enabled)
    {
        auto& value = capability(id);
        if(value.enabled == enabled)
        {
            return;
        }
        value.enabled = enabled;
        if(!enabled)
        {
            cancel_interaction(id);
        }
        invalidate(id, {.paint = true, .interaction = true});
    }

    void set_activation(node_id id, std::function<void()> callback)
    {
        auto& value = capability(id);
        if(!value.activate && !callback)
        {
            return;
        }
        auto replacement = callback ? std::make_shared<std::function<void()>>(std::move(callback)) : nullptr;
        value.activate.swap(replacement);
        invalidate(id, {.interaction = true});
    }

    [[nodiscard]] visual_state appearance(node_id id, visual_state inherited) const
    {
        const auto& capability = get(id).content.interaction;
        if(inherited == visual_state::disabled || (capability && !capability->enabled))
        {
            return visual_state::disabled;
        }
        if(!capability)
        {
            return inherited;
        }
        if(interaction.captured == id && interaction.hovered == id)
        {
            return visual_state::pressed;
        }
        if(interaction.hovered == id)
        {
            return visual_state::hovered;
        }
        if(interaction.focused == id)
        {
            return visual_state::focused;
        }
        return visual_state::normal;
    }

    // Dispatch may retain a runtime while its public owner is replaced.
    // Retirement makes all handles invalid immediately without destroying an
    // executing callback's separately retained callable.
    void retire() noexcept
    {
        _retired = true;
        _root.reset();
        interaction.hovered.reset();
        interaction.focused.reset();
        interaction.captured.reset();
        paint.invalidate();
    }

    static std::shared_ptr<document_state> owner_of(const std::shared_ptr<element_binding>& binding)
    {
        if(!binding)
        {
            throw invalid_element{};
        }
        auto owner = binding->owner.lock();
        if(!owner)
        {
            throw invalid_element{};
        }
        (void)owner->get(binding->id);
        return owner;
    }

    input_state             interaction;
    bool                    mounted = false;
    text_service            text;
    layout_engine           layout;
    painter                 paint;
    preparation_diagnostics diagnostics;
    geometry_inputs         geometry;

private:
    document_record& container(node_id id) const
    {
        auto& record = get(id);
        if(!primitive_has_children(record.content) || !record.content.accepts_children)
        {
            throw std::invalid_argument("Parent must be a container");
        }
        return record;
    }

    static std::size_t insertion_index(std::size_t index, std::size_t size)
    {
        if(index == end)
        {
            return size;
        }
        if(index > size)
        {
            throw std::out_of_range("Child insertion index");
        }
        return index;
    }

    void set_target(std::optional<node_id>& current, std::optional<node_id> next)
    {
        if(current == next)
        {
            return;
        }
        const auto previous = current;
        current = next;
        if(previous && contains(*previous))
        {
            invalidate(*previous, {.paint = true});
        }
        if(next && contains(*next))
        {
            invalidate(*next, {.paint = true});
        }
    }

    interaction_capability& capability(node_id id)
    {
        auto& value = get(id).content.interaction;
        if(!value)
        {
            throw std::logic_error("Element has no interaction capability");
        }
        return *value;
    }

    const std::thread::id      _thread = std::this_thread::get_id();
    std::vector<document_slot> _slots;
    std::vector<std::size_t>   _free_slots;
    std::optional<node_id>     _root;
    invalidation               _pending;
    bool                       _retired = false;
};

inline document_record& resolve(const std::shared_ptr<element_binding>& binding)
{
    return document_state::owner_of(binding)->get(binding->id);
}

inline void invalidate(const std::shared_ptr<element_binding>& binding, invalidation changes)
{
    document_state::owner_of(binding)->invalidate(binding->id, changes);
}

inline construction_owner::~construction_owner()
{
    if(state)
    {
        state->retire();
    }
}

} // namespace centui::details

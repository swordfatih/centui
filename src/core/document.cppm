module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

export module centui.document;

export import centui.element;
import centui.node;
import centui.glyph;
import centui.draw;

export namespace centui
{

namespace details { struct document_access; }

// Sole owner of mounted widgets. Handles and callbacks must not own this store.
class document
{
public:
    static constexpr std::size_t end = std::numeric_limits<std::size_t>::max();

    document() : _store(std::make_shared<details::document_store>()) {}
    explicit document(const element& root) : document() { mount(root); }
    document(const document&) = delete;
    document& operator=(const document&) = delete;
    document(document&& other)
    {
        (void)other.store();
        _store = std::move(other._store);
    }
    document& operator=(document&& other)
    {
        if(this != &other)
        {
            if(_store) _store->check_thread();
            (void)other.store();
            _store = std::move(other._store);
        }
        return *this;
    }

    [[nodiscard]] bool empty() const { return !store().root; }

    [[nodiscard]] invalidation pending_changes() const
    {
        auto result = store().pending;
        for(const auto& slot : store().slots)
            if(slot.record) result.merge(slot.record->binding->pending);
        return result;
    }

    [[nodiscard]] element root() const
    {
        if(!store().root) throw std::logic_error("Document has no root");
        return details::element_access::alias(store().get(*store().root).binding);
    }

    void mount(const element& value)
    {
        if(store().root) throw std::logic_error("Document already has a root");
        store().root = adopt(value);
    }

    [[nodiscard]] std::vector<element> children(const element& parent) const
    {
        (void)checked(parent);
        return details::element_access::children(parent);
    }

    [[nodiscard]] std::optional<element> parent(const element& child) const
    {
        const auto& record = checked(child);
        if(!record.parent) return std::nullopt;
        return details::element_access::alias(store().get(*record.parent).binding);
    }

    // Insert only unmounted construction trees. Existing widgets use reparent.
    void insert(const element& parent, const element& child, std::size_t index = end)
    {
        auto& target = checked_container(parent);
        const auto position = insertion_index(index, target.children.size());
        // Allocation may fail, but ownership and topology have not changed yet.
        target.children.reserve(target.children.size() + 1);
        const auto id = adopt(child);
        detach_layouts();
        target.children.insert(target.children.begin() + position, id);
        store().get(id).parent = parent.id();
        details::invalidate(details::element_access::binding(parent),
                            {.topology = true, .layout = true, .paint = true, .interaction = true});
    }

    void append(const element& parent, const element& child) { insert(parent, child); }

    // Index is the destination position after removing the child from its old parent.
    void reparent(const element& child, const element& parent, std::size_t index = end)
    {
        auto& source = checked(child);
        auto& target = checked_container(parent);
        if(!source.parent) throw std::invalid_argument("Cannot reparent the document root");
        for(auto ancestor = std::optional{parent.id()}; ancestor; ancestor = store().get(*ancestor).parent)
            if(*ancestor == child.id()) throw std::invalid_argument("Reparenting would create a cycle");

        const bool same_parent = *source.parent == parent.id();
        const auto position = insertion_index(index, target.children.size() - (same_parent ? 1 : 0));
        const auto old_parent = *source.parent;
        if(same_parent && target.children[position] == child.id()) return;
        target.children.reserve(target.children.size() + (same_parent ? 0 : 1));
        auto& old = store().get(*source.parent).children;
        detach_layouts();
        std::erase(old, child.id());
        target.children.insert(target.children.begin() + position, child.id());
        source.parent = parent.id();
        const invalidation changes{.topology = true, .layout = true, .paint = true, .interaction = true};
        details::invalidate(store().get(old_parent).binding, changes);
        details::invalidate(details::element_access::binding(parent), changes);
    }

    // Order must be an exact permutation of the existing children.
    void reorder(const element& parent, std::span<const element> order)
    {
        auto& target = checked_container(parent);
        if(order.size() != target.children.size()) throw std::invalid_argument("Incomplete child order");
        std::vector<node_id> ids;
        ids.reserve(order.size());
        for(const auto& child : order)
        {
            const auto& record = checked(child);
            if(record.parent != parent.id() || std::ranges::find(ids, child.id()) != ids.end())
                throw std::invalid_argument("Order contains a foreign or duplicate child");
            ids.push_back(child.id());
        }
        if(ids == target.children) return;
        detach_layouts();
        target.children.swap(ids);
        details::invalidate(details::element_access::binding(parent),
                            {.topology = true, .layout = true, .paint = true, .interaction = true});
    }

    // Removal destroys a subtree immediately. It does not return an owning handle.
    void remove(const element& child)
    {
        const auto id = child.id();
        auto& record = checked(child);
        std::vector<node_id> removed;
        auto collect = [&](this auto&& self, node_id current) -> void
        {
            removed.push_back(current);
            for(auto next : store().get(current).children) self(next);
        };
        collect(id);
        store().free_slots.reserve(store().free_slots.size() + removed.size());
        detach_layouts();
        const invalidation changes{.topology = true, .layout = true, .paint = true, .interaction = true};
        if(record.parent)
        {
            auto& parent = store().get(*record.parent);
            std::erase(parent.children, id);
            details::invalidate(parent.binding, changes);
        }
        else
        {
            store().root.reset();
            store().pending.merge(changes);
        }
        for(auto current : removed)
        {
            auto& slot = store().slots[current.index];
            if(auto* button = std::get_if<details::button_data>(&slot.record->value->data))
            {
                button->interaction->enabled = false;
                button->interaction->hovered = false;
                button->interaction->focused = false;
                button->interaction->pressed = false;
            }
            slot.record.reset();
            // Retire exhausted generations rather than wrapping and reviving an ID.
            if(slot.generation != std::numeric_limits<std::uint64_t>::max())
            {
                ++slot.generation;
                store().free_slots.push_back(current.index);
            }
        }
    }

private:
    friend struct details::document_access;
    std::shared_ptr<details::document_store> _store;

    details::document_store& store() const
    {
        if(!_store) throw std::logic_error("Document is moved from");
        _store->check_thread();
        return *_store;
    }

    details::document_record& checked(const element& value) const
    {
        (void)store();
        const auto& binding = details::element_access::binding(value);
        (void)details::resolve(binding);
        if(binding->owner.lock() != _store)
            throw std::invalid_argument("Element does not belong to this document");
        return store().get(binding->id);
    }

    details::document_record& checked_container(const element& value) const
    {
        auto& record = checked(value);
        if(value.kind() != element_kind::container) throw std::invalid_argument("Parent must be a container");
        return record;
    }

    static std::size_t insertion_index(std::size_t index, std::size_t size)
    {
        if(index == end) return size;
        if(index > size) throw std::out_of_range("Child insertion index");
        return index;
    }

    void detach_layouts()
    {
        for(auto& slot : store().slots)
            if(slot.record) slot.record->value->layout.clear_children();
    }

    node_id adopt(const element& root)
    {
        const auto& root_binding = details::element_access::binding(root);
        (void)details::resolve(root_binding);
        if(!root_binding->draft || !root_binding->draft_parent.expired() || root.mounted())
            throw std::invalid_argument("Construction tree already has an owner");

        struct pending
        {
            node* source;
            std::shared_ptr<details::element_binding> binding;
            std::optional<std::size_t> parent;
            node_id id;
            std::unique_ptr<details::document_record> record;
        };
        std::vector<pending> plan;
        auto collect = [&](this auto&& self, node& value, std::optional<std::size_t> parent) -> void
        {
            auto binding = std::static_pointer_cast<details::element_binding>(value.identity.lock());
            if(!binding || !binding->owner.expired()) throw std::invalid_argument("Invalid construction identity");
            if(std::ranges::any_of(plan, [&](const pending& item) { return item.binding == binding; }))
                throw std::invalid_argument("Duplicate construction identity");
            if(!std::holds_alternative<container_data>(value.data) && !value.children.empty())
                throw std::invalid_argument("Only containers may have children");
            binding->check_thread();
            value.style.validate();
            const auto index = plan.size();
            auto record = std::make_unique<details::document_record>();
            record->value = std::make_unique<node>();
            record->binding = binding;
            record->children.reserve(value.children.size());
            plan.push_back({&value, std::move(binding), parent, {}, std::move(record)});
            for(auto& child : value.children) self(child, index);
        };
        collect(*root_binding->draft, std::nullopt);

        auto& runtime = store();
        const auto reused = std::min(plan.size(), runtime.free_slots.size());
        const auto old_size = runtime.slots.size();
        runtime.slots.reserve(old_size + plan.size() - reused);
        for(std::size_t i = 0; i < plan.size(); ++i)
        {
            const auto index = i < reused ? runtime.free_slots[runtime.free_slots.size() - 1 - i] : old_size + i - reused;
            plan[i].id = {index, i < reused ? runtime.slots[index].generation : 1};
        }
        for(auto& item : plan)
            if(item.parent)
            {
                item.record->parent = plan[*item.parent].id;
                plan[*item.parent].record->children.push_back(item.id);
            }

        // Commit: all allocations/validation are complete. No raw child vectors
        // are moved into the registry; only payload, style, and layout survive.
        for(auto& item : plan) item.source->layout.clear_children();
        for(auto& item : plan)
        {
            item.record->value->data = std::move(item.source->data);
            item.record->value->style = item.source->style;
            item.record->value->layout = std::move(item.source->layout);
            item.record->value->identity = item.binding;
        }
        runtime.slots.resize(old_size + plan.size() - reused);
        runtime.free_slots.resize(runtime.free_slots.size() - reused);
        for(auto& item : plan)
        {
            runtime.slots[item.id.index].record = std::move(item.record);
            item.binding->owner = _store;
            item.binding->id = item.id;
            item.binding->pending.merge({.topology = true,
                                         .text = !std::holds_alternative<container_data>(runtime.get(item.id).value->data),
                                         .layout = true, .paint = true, .interaction = true});
            item.binding->draft_parent.reset();
            item.binding->draft_children.clear();
        }
        root_binding->draft.reset();
        return plan.front().id;
    }
};

namespace details
{
struct document_access
{
    static void complete_empty(document& owner, invalidation completed)
    {
        auto& store = owner.store();
        if(!store.root) store.pending.clear(completed);
    }
};
}

std::vector<draw_command> prepare(document& owner, font& face, int width, int height)
{
    if(owner.empty())
    {
        if(width > 0 && height > 0)
            details::document_access::complete_empty(owner, {.topology = true, .text = true, .layout = true, .paint = true});
        return {};
    }
    auto root = owner.root();
    return prepare(root, face, width, height);
}

} // namespace centui

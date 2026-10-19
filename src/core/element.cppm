module;

#include <algorithm>
#include <concepts>
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

export module centui.element;

import centui.node;
import centui.style;
import centui.glyph;
import centui.draw;

export namespace centui
{

struct node_id
{
    std::size_t index = std::numeric_limits<std::size_t>::max();
    std::uint64_t generation = 0;
    constexpr bool operator==(const node_id&) const = default;
};

enum class element_kind { container, text, button };

class invalid_element : public std::logic_error
{
public:
    invalid_element() : std::logic_error("Element is moved from, removed, or its owner has expired") {}
};

class element;

namespace details
{
struct document_store;
struct element_access;
struct handle_tag {};

struct element_binding : construction_identity
{
    std::unique_ptr<node> draft;
    std::weak_ptr<element_binding> draft_parent;
    std::vector<std::shared_ptr<element_binding>> draft_children;
    std::weak_ptr<document_store> owner;
    node_id id;
};

struct document_record
{
    std::unique_ptr<node> value;
    std::shared_ptr<element_binding> binding;
    std::optional<node_id> parent;
    std::vector<node_id> children;
};

struct document_slot
{
    std::uint64_t generation = 1;
    std::unique_ptr<document_record> record;
};

struct document_store
{
    std::vector<document_slot> slots;
    std::vector<std::size_t> free_slots;
    std::optional<node_id> root;

    document_record& get(node_id id) const
    {
        if(id.index >= slots.size() || slots[id.index].generation != id.generation || !slots[id.index].record)
            throw invalid_element{};
        return *slots[id.index].record;
    }

    ~document_store()
    {
        // Detach Yoga edges before independently owned nodes are destroyed.
        for(auto& slot : slots)
            if(slot.record)
            {
                slot.record->value->layout.clear_children();
                if(auto* button = std::get_if<button_data>(&slot.record->value->data))
                {
                    button->interaction->enabled = false;
                    button->interaction->hovered = false;
                    button->interaction->focused = false;
                    button->interaction->pressed = false;
                }
            }
    }
};

inline node& resolve(const std::shared_ptr<element_binding>& binding)
{
    if(!binding) throw invalid_element{};
    if(binding->draft) return *binding->draft;
    if(auto parent = binding->draft_parent.lock())
    {
        auto& value = resolve(parent);
        for(auto& child : value.children)
            if(child.identity.lock() == binding) return child;
        throw invalid_element{};
    }
    if(auto owner = binding->owner.lock()) return *owner->get(binding->id).value;
    throw invalid_element{};
}
}

// Copies alias one widget. Once mounted, handles do not own widget lifetime.
class element
{
public:
    element(const element&) = default;
    element& operator=(const element&) = default;
    element(element&&) noexcept = default;
    element& operator=(element&&) noexcept = default;

    [[nodiscard]] bool valid() const noexcept
    {
        try { (void)details::resolve(_binding); return true; }
        catch(const invalid_element&) { return false; }
    }

    [[nodiscard]] bool mounted() const
    {
        (void)details::resolve(_binding);
        return !_binding->owner.expired();
    }

    [[nodiscard]] node_id id() const
    {
        (void)details::resolve(_binding);
        if(_binding->owner.expired()) throw std::logic_error("Element is not mounted");
        return _binding->id;
    }

    [[nodiscard]] element_kind kind() const
    {
        return static_cast<element_kind>(details::resolve(_binding).data.index());
    }

    template <typename widget>
        requires std::derived_from<widget, element>
    [[nodiscard]] widget as() const
    {
        if(kind() != widget::widget_kind) throw std::invalid_argument("Wrong element kind");
        return widget(details::handle_tag{}, _binding);
    }

    [[nodiscard]] centui::style& style() { return details::resolve(_binding).style; }
    [[nodiscard]] const centui::style& style() const { return details::resolve(_binding).style; }
    [[nodiscard]] bool operator==(const element& other) const noexcept { return _binding == other._binding; }

protected:
    explicit element(node value) : _binding(std::make_shared<details::element_binding>())
    {
        _binding->draft = std::make_unique<node>(std::move(value));
        _binding->draft->identity = _binding;
    }
    element(details::handle_tag, std::shared_ptr<details::element_binding> binding) : _binding(std::move(binding)) {}

    // Value-tree children are a construction adapter; mounted edges use IDs.
    void compose(std::span<const element> children)
    {
        auto& target = details::resolve(_binding);
        if(!_binding->draft || !target.children.empty() || !std::holds_alternative<container_data>(target.data))
            throw std::logic_error("Composition requires an empty unmounted container");
        std::vector<std::shared_ptr<details::element_binding>> bindings;
        bindings.reserve(children.size());
        for(const auto& child : children)
        {
            (void)details::resolve(child._binding);
            if(!child._binding->draft || !child._binding->draft_parent.expired() || child.mounted())
                throw std::invalid_argument("Child already has an owner");
            if(child._binding == _binding || std::ranges::find(bindings, child._binding) != bindings.end())
                throw std::invalid_argument("Duplicate child or cycle");
            bindings.push_back(child._binding);
        }
        target.children.reserve(bindings.size());
        _binding->draft_children.reserve(bindings.size());
        // All allocation and validation precede ownership changes.
        for(const auto& binding : bindings)
        {
            target.children.push_back(std::move(*binding->draft));
            binding->draft.reset();
            binding->draft_parent = _binding;
            _binding->draft_children.push_back(binding);
        }
    }

private:
    friend struct details::element_access;
    std::shared_ptr<details::element_binding> _binding;
};

namespace details
{
// Transitional access for raw-node tests and the existing preparation API.
struct element_access
{
    static const std::shared_ptr<element_binding>& binding(const element& value) { return value._binding; }
    static node& get(const element& value) { return resolve(value._binding); }
    static element alias(std::shared_ptr<element_binding> binding) { return {handle_tag{}, std::move(binding)}; }
    static std::vector<element> children(const element& value)
    {
        auto& current = resolve(value._binding);
        std::vector<element> result;
        if(auto owner = value._binding->owner.lock())
        {
            for(auto id : owner->get(value._binding->id).children)
                result.push_back(alias(owner->get(id).binding));
        }
        else
        {
            for(auto& child : current.children)
            {
                auto binding = std::static_pointer_cast<element_binding>(child.identity.lock());
                if(!binding) throw std::logic_error("Raw construction node has no element identity");
                result.push_back(alias(std::move(binding)));
            }
        }
        return result;
    }
};

inline void attach_elements(const element& root, font& face)
{
    auto& value = element_access::get(root);
    if(!root.mounted()) { value.attach(face); return; }
    value.layout.clear_children();
    value.attach(face);
    for(const auto& child : element_access::children(root))
    {
        attach_elements(child, face);
        value.layout.insert_child(element_access::get(child).layout);
    }
}

inline void append_commands(const element& root, float x, float y, std::vector<draw_command>& output)
{
    const auto& value = element_access::get(root);
    // Mounted payloads have no value-tree children. Reuse the existing painter.
    auto list = commands(value);
    for(auto& command : list)
    {
        std::visit([&](auto& item) { item.bounds.x += x; item.bounds.y += y; }, command);
        output.push_back(std::move(command));
    }
    if(root.mounted())
    {
        const auto bounds = value.layout.bounds();
        for(const auto& child : element_access::children(root))
            append_commands(child, x + bounds.x, y + bounds.y, output);
    }
}
}

// Commands still borrow the document/construction tree until its next mutation.
std::vector<draw_command> prepare(element& root, font& face, int width, int height)
{
    if(width <= 0 || height <= 0) return {};
    details::attach_elements(root, face);
    details::element_access::get(root).calculate(static_cast<float>(width), static_cast<float>(height));
    std::vector<draw_command> result;
    details::append_commands(root, 0, 0, result);
    return result;
}

} // namespace centui

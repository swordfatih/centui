module;

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

export module centui.element;

export import centui.model;
export import centui.style;
export import centui.diagnostics;
import centui.runtime;
import centui.preparation;
import centui.glyph;
import centui.draw;
import centui.unit;

export namespace centui
{

namespace details
{
struct element_access;
struct handle_tag
{
};
} // namespace details

// Copies alias one widget. Only a construction root owns its runtime;
// after mounting, widget lifetime belongs exclusively to the document.
class element
{
public:
    element(const element&) = default;
    element& operator=(const element&) = default;
    element(element&&) noexcept = default;
    element& operator=(element&&) noexcept = default;

    [[nodiscard]] bool valid() const
    {
        try
        {
            (void)details::resolve(_binding);
            return true;
        }
        catch(const invalid_element&)
        {
            return false;
        }
    }

    [[nodiscard]] bool mounted() const
    {
        return details::document_state::owner_of(_binding)->mounted;
    }

    [[nodiscard]] node_id id() const
    {
        if(!mounted())
        {
            throw std::logic_error("Element is not mounted");
        }
        return _binding->id;
    }

    [[nodiscard]] element_kind kind() const
    {
        return static_cast<element_kind>(details::resolve(_binding).content.data.index());
    }

    template <typename widget>
        requires std::derived_from<widget, element>
    [[nodiscard]] widget as() const
    {
        if(kind() != widget::widget_kind)
        {
            throw std::invalid_argument("Wrong element kind");
        }
        return widget(details::handle_tag{}, *this);
    }

    [[nodiscard]] const centui::style& style() const
    {
        return details::resolve(_binding).content.style;
    }

    [[nodiscard]] invalidation pending_changes() const
    {
        return details::resolve(_binding).pending;
    }

    void enable_preparation_diagnostics(bool enabled = true)
    {
        details::document_state::owner_of(_binding)->diagnostics.enable(enabled);
    }

    [[nodiscard]] preparation_statistics preparation_stats() const
    {
        return details::document_state::owner_of(_binding)->diagnostics.snapshot();
    }

    void set_style(centui::style replacement)
    {
        auto& current = details::resolve(_binding).content.style;
        replacement.validate();
        const auto changes = replacement.changes_from(current);
        if(!changes.any())
        {
            return;
        }
        current = replacement;
        changed(changes);
    }

    void patch_style(const style_patch& patch)
    {
        set_style(patch.apply(style()));
    }

    [[nodiscard]] bool operator==(const element& other) const noexcept
    {
        return _binding == other._binding;
    }

protected:
    explicit element(details::element_content content)
        : _construction(std::make_shared<details::construction_owner>())
    {
        _construction->state = std::make_shared<details::document_state>();
        _binding = _construction->state->create_root(std::move(content));
        _binding->construction = _construction;
    }

    element(details::handle_tag, std::shared_ptr<details::element_binding> binding)
        : _binding(std::move(binding)), _construction(_binding->construction.lock())
    {
    }

    void changed(invalidation changes)
    {
        details::invalidate(_binding, changes);
    }

    void set_text_value(std::string value)
    {
        auto& payload = details::resolve(_binding).content.data;
        auto* label = std::get_if<details::text_content>(&payload);
        auto* button = std::get_if<details::button_content>(&payload);
        if(!label && !button)
        {
            throw std::invalid_argument("Element has no text value");
        }
        auto& current = label ? label->value : button->value;
        if(current == value)
        {
            return;
        }
        current.swap(value);
        changed({.text = true, .layout = true, .paint = true});
    }

    void compose(std::span<const element> children)
    {
        auto        owner = details::document_state::owner_of(_binding);
        const auto& record = owner->get(_binding->id);
        if(owner->mounted || owner->root() != _binding->id || !record.children.empty() ||
           kind() != element_kind::container)
        {
            throw std::logic_error("Composition requires an empty construction root container");
        }

        std::vector<std::shared_ptr<details::element_binding>> bindings;
        bindings.reserve(children.size());
        for(const auto& child: children)
        {
            bindings.push_back(child._binding);
        }
        owner->insert(_binding->id, bindings);
    }

private:
    friend struct details::element_access;
    std::shared_ptr<details::element_binding>    _binding;
    std::shared_ptr<details::construction_owner> _construction;
};

namespace details
{

struct element_access
{
    static const std::shared_ptr<element_binding>& binding(const element& value)
    {
        return value._binding;
    }

    static std::shared_ptr<document_state> state(const element& value)
    {
        return document_state::owner_of(value._binding);
    }

    static element_content& get(const element& value)
    {
        return resolve(value._binding).content;
    }

    static element alias(std::shared_ptr<element_binding> binding)
    {
        return {handle_tag{}, std::move(binding)};
    }

    static void complete(const element& value, invalidation completed)
    {
        state(value)->complete(value._binding->id, completed);
    }

    static rect bounds(const element& value)
    {
        return state(value)->layout.bounds(value._binding->id.index);
    }

    static std::vector<element> children(const element& value)
    {
        auto                 owner = state(value);
        const auto&          record = owner->get(value._binding->id);
        std::vector<element> result;
        result.reserve(record.children.size());
        for(const auto id: record.children)
        {
            result.push_back(alias(owner->get(id).binding));
        }
        return result;
    }
};

inline void prepare_geometry(const element& root, font& face, int width, int height)
{
    prepare_geometry(*element_access::state(root), face, width, height);
}

inline std::pair<float, float> parent_offset(const element& value)
{
    auto       state = element_access::state(value);
    const auto parent = state->get(element_access::binding(value)->id).parent;
    if(!parent)
    {
        return {0, 0};
    }
    const auto bounds = state->absolute_bounds(*parent);
    return {bounds.x, bounds.y};
}

inline std::span<const draw_command> prepared_commands(const element& root, font& face, int width, int height)
{
    if(width <= 0 || height <= 0)
    {
        return {};
    }

    auto state = element_access::state(root);
    return prepared_commands(*state, element_access::binding(root)->id, face, width, height);
}

} // namespace details

// Command records borrow the runtime's text until mutation/preparation/destruction.
std::vector<draw_command> prepare(element& root, font& face, int width, int height)
{
    const auto list = details::prepared_commands(root, face, width, height);
    return {list.begin(), list.end()};
}

} // namespace centui

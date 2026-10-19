module;

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

export module centui.document;

export import centui.element;
import centui.runtime;
import centui.glyph;
import centui.draw;

export namespace centui
{

namespace details
{
struct document_access;
}

// Sole owner of a mounted runtime. Typed handles retain only widget identity.
class document
{
public:
    static constexpr std::size_t end = details::document_state::end;

    document() : _state(std::make_shared<details::document_state>())
    {
        _state->mounted = true;
    }

    explicit document(const element& root) : document()
    {
        mount(root);
    }

    ~document()
    {
        if(_state)
        {
            _state->retire();
        }
    }

    document(const document&) = delete;
    document& operator=(const document&) = delete;

    document(document&& other)
    {
        (void)other.state();
        _state = std::move(other._state);
    }

    document& operator=(document&& other)
    {
        if(this != &other)
        {
            if(_state)
            {
                _state->check_thread();
            }
            (void)other.state();
            if(_state)
            {
                _state->retire();
            }
            _state = std::move(other._state);
        }
        return *this;
    }

    [[nodiscard]] bool empty() const
    {
        return !state().root();
    }

    [[nodiscard]] invalidation pending_changes() const
    {
        return state().pending();
    }

    void enable_preparation_diagnostics(bool enabled = true)
    {
        state().diagnostics.enable(enabled);
    }

    [[nodiscard]] preparation_statistics preparation_stats() const
    {
        return state().diagnostics.snapshot();
    }

    [[nodiscard]] element root() const
    {
        const auto id = state().root();
        if(!id)
        {
            throw std::logic_error("Document has no root");
        }
        return details::element_access::alias(state().get(*id).binding);
    }

    void mount(const element& value)
    {
        if(!empty())
        {
            throw std::logic_error("Document already has a root");
        }

        auto        source = details::element_access::state(value);
        const auto& binding = details::element_access::binding(value);
        if(source->mounted || source->root() != binding->id)
        {
            throw std::invalid_argument("Construction tree already has an owner");
        }
        auto construction = binding->construction.lock();
        if(!construction || construction->state != source)
        {
            throw std::logic_error("Construction root has no owner");
        }
        if(state().diagnostics.enabled())
        {
            source->diagnostics.enable(true);
        }

        _state->retire();
        _state = std::move(source);
        _state->mounted = true;
        construction->state.reset();
    }

    [[nodiscard]] std::vector<element> children(const element& parent) const
    {
        const auto           id = checked(parent);
        std::vector<element> result;
        for(const auto child: state().get(id).children)
        {
            const auto& record = state().get(child);
            if(!record.content.private_part)
            {
                result.push_back(details::element_access::alias(record.binding));
            }
        }
        return result;
    }

    [[nodiscard]] std::optional<element> parent(const element& child) const
    {
        const auto id = state().get(checked(child)).parent;
        if(!id)
        {
            return std::nullopt;
        }
        return details::element_access::alias(state().get(*id).binding);
    }

    void insert(const element& parent, const element& child, std::size_t index = end)
    {
        const auto  id = checked(parent);
        const auto& binding = details::element_access::binding(child);
        state().insert(id, std::span{std::addressof(binding), std::size_t{1}}, index);
    }

    void append(const element& parent, const element& child)
    {
        insert(parent, child);
    }

    // Index is the destination position after removing the existing child.
    void reparent(const element& child, const element& parent, std::size_t index = end)
    {
        const auto child_id = checked(child);
        const auto parent_id = checked(parent);
        state().reparent(child_id, parent_id, index);
    }

    void reorder(const element& parent, std::span<const element> order)
    {
        const auto           parent_id = checked(parent);
        std::vector<node_id> ids;
        ids.reserve(order.size());
        for(const auto& child: order)
        {
            ids.push_back(checked(child));
        }
        state().reorder(parent_id, ids);
    }

    void remove(const element& child)
    {
        state().remove(checked(child));
    }

private:
    friend struct details::document_access;
    std::shared_ptr<details::document_state> _state;

    details::document_state& state() const
    {
        if(!_state)
        {
            throw std::logic_error("Document is moved from");
        }
        _state->check_thread();
        return *_state;
    }

    node_id checked(const element& value) const
    {
        (void)state();
        if(details::element_access::state(value) != _state)
        {
            throw std::invalid_argument("Element does not belong to this document");
        }
        const auto id = details::element_access::binding(value)->id;
        if(state().get(id).content.private_part)
        {
            throw std::invalid_argument("Component parts are not public document children");
        }
        return id;
    }
};

namespace details
{

struct document_access
{
    static std::shared_ptr<document_state> current(document& owner)
    {
        if(owner._state)
        {
            owner._state->check_thread();
        }
        return owner._state;
    }

    static std::shared_ptr<document_state> state(document& owner)
    {
        (void)owner.state();
        return owner._state;
    }

    static void complete_empty(document& owner, invalidation completed)
    {
        owner.state().complete_empty(completed);
    }
};

} // namespace details

std::vector<draw_command> prepare(document& owner, font& face, int width, int height)
{
    if(owner.empty())
    {
        if(width > 0 && height > 0)
        {
            details::document_access::complete_empty(owner, {.topology = true, .text = true, .layout = true, .paint = true});
        }
        return {};
    }
    auto root = owner.root();
    return prepare(root, face, width, height);
}

} // namespace centui

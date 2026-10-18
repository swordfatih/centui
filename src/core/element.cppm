export module centui.element;

import std;
import centui.node;
import centui.style;
import centui.glyph;
import centui.draw;

export namespace centui
{

namespace details
{
struct element_access;
}

// A value-owned tree, not a polymorphic widget interface.
class element
{
public:
    element(const element&) = delete;
    element& operator=(const element&) = delete;
    element(element&&) noexcept = default;
    element& operator=(element&&) noexcept = default;

    [[nodiscard]] centui::style& style() noexcept
    {
        return _node.style;
    }

    [[nodiscard]] const centui::style& style() const noexcept
    {
        return _node.style;
    }

protected:
    explicit element(node value) : _node(std::move(value))
    {
    }

private:
    friend struct details::element_access;
    node _node;
};

namespace details
{

// Internal construction/testing access, not part of the application API.
struct element_access
{
    static node& get(element& value) noexcept
    {
        return value._node;
    }

    static const node& get(const element& value) noexcept
    {
        return value._node;
    }
};

} // namespace details

// CPU-only preparation. Returned commands borrow root until its next mutation.
// No element address or font reference is retained between calls.
std::vector<draw_command> prepare(element& root, font& face, int width, int height)
{
    if(width <= 0 || height <= 0)
    {
        return {};
    }

    auto& tree = details::element_access::get(root);
    tree.attach(face);
    tree.calculate(static_cast<float>(width), static_cast<float>(height));
    return commands(tree);
}

} // namespace centui

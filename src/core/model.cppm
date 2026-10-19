module;

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

export module centui.model;

import centui.style;

export namespace centui
{

struct node_id
{
    std::size_t    index = std::numeric_limits<std::size_t>::max();
    std::uint64_t  generation = 0;
    constexpr bool operator==(const node_id&) const = default;
};

// Public handle identity, independent of runtime primitive behavior.
enum class element_kind
{
    container,
    text,
    button
};

class invalid_element : public std::logic_error
{
public:
    invalid_element() : std::logic_error("Element is moved from, removed, or its owner has expired")
    {
    }
};

struct container_data
{
};

namespace details
{

enum class visual_state
{
    normal,
    hovered,
    pressed,
    focused,
    disabled
};

struct interaction_capability
{
    bool                                   enabled = true;
    std::shared_ptr<std::function<void()>> activate = nullptr;
};

struct text_content
{
    std::string value;
};

struct element_content
{
    std::variant<container_data, text_content> data = container_data{};
    centui::style                              style;
    element_kind                               kind = element_kind::container;
    std::optional<interaction_capability>      interaction = std::nullopt;
    bool                                       accepts_children = true;
    bool                                       private_part = false;
    bool                                       inherit_text_style = false;
};

} // namespace details
} // namespace centui

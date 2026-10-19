module;

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>

export module centui.model;

import centui.style;

export namespace centui
{

struct node_id
{
    std::size_t   index = std::numeric_limits<std::size_t>::max();
    std::uint64_t generation = 0;

    constexpr bool operator==(const node_id&) const = default;
};

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

struct element_identity
{
};

struct button_interaction_state
{
    std::weak_ptr<element_identity>        identity;
    bool                                   hovered = false;
    bool                                   pressed = false;
    bool                                   focused = false;
    bool                                   enabled = true;
    std::shared_ptr<std::function<void()>> on_click;
};

struct text_content
{
    std::string value;
};

struct button_content
{
    std::string                               value;
    std::shared_ptr<button_interaction_state> interaction;
};

// Authoritative widget inputs. Prepared text and geometry belong to services.
struct element_content
{
    std::variant<container_data, text_content, button_content> data = container_data{};
    centui::style                                              style;
};

} // namespace details
} // namespace centui

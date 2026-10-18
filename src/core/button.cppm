export module centui.button;

import std;
import centui.element;
import centui.node;
import centui.style;

export namespace centui
{

struct button_options
{
    bool enabled = true;
    std::function<void()> on_click;
};

namespace details
{

} // namespace details

template <typename... styles>
class button : public element
{
public:
    explicit button(std::string value, button_options options = {})
        : element(make_node(std::move(value), std::move(options)))
    {
    }

    void set_enabled(bool enabled) noexcept
    {
        auto& state = interaction();
        state.enabled = enabled;
        if(!enabled)
        {
            state.hovered = false;
            state.pressed = false;
            state.focused = false;
        }
    }

    [[nodiscard]] bool enabled() const noexcept
    {
        return interaction().enabled;
    }

private:
    static node make_node(std::string value, button_options options)
    {
        auto interaction = std::shared_ptr<details::button_interaction_state>{std::make_unique<details::button_interaction_state>()};
        interaction->enabled = options.enabled;
        interaction->on_click = std::move(options.on_click);
        return node{
            .children = {},
            .data = details::button_data{
                .value = std::move(value),
                .shaped = {},
                .interaction = std::move(interaction)},
            .style = style::resolve<styles...>(),
        };
    }

    [[nodiscard]] details::button_interaction_state& interaction() noexcept
    {
        auto& tree = details::element_access::get(*this);
        return *std::get<details::button_data>(tree.data).interaction;
    }

    [[nodiscard]] const details::button_interaction_state& interaction() const noexcept
    {
        const auto& tree = details::element_access::get(*this);
        return *std::get<details::button_data>(tree.data).interaction;
    }
};

} // namespace centui

module;

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

export module centui.button;

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

template <typename... styles>
class button : public element
{
public:
    static constexpr element_kind widget_kind = element_kind::button;
    explicit button(std::string value, button_options options = {})
        : element(make_node(std::move(value), std::move(options)))
    {
    }

    void set_enabled(bool enabled)
    {
        auto& state = interaction();
        if(state.enabled == enabled) return;
        state.enabled = enabled;
        if(!enabled)
        {
            state.hovered = false;
            state.pressed = false;
            state.focused = false;
        }
        changed({.paint = true, .interaction = true});
    }

    [[nodiscard]] bool enabled() const
    {
        return interaction().enabled;
    }

    void set_text(std::string value) { set_text_value(std::move(value)); }
    void value(std::string value) { set_text(std::move(value)); }
    [[nodiscard]] std::string_view value() const
    {
        return std::get<details::button_data>(details::element_access::get(*this).data).value;
    }

    void set_on_click(std::function<void()> callback)
    {
        auto& state = interaction();
        if(!state.on_click && !callback) return;
        state.on_click.swap(callback);
        changed({.interaction = true});
    }

private:
    friend class element;
    button(details::handle_tag tag, std::shared_ptr<details::element_binding> binding)
        : element(tag, std::move(binding)) {}

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

    [[nodiscard]] details::button_interaction_state& interaction()
    {
        auto& tree = details::element_access::get(*this);
        return *std::get<details::button_data>(tree.data).interaction;
    }

    [[nodiscard]] const details::button_interaction_state& interaction() const
    {
        const auto& tree = details::element_access::get(*this);
        return *std::get<details::button_data>(tree.data).interaction;
    }
};

} // namespace centui

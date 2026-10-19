module;

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

export module centui.button;

import centui.element;
import centui.model;
import centui.style;

export namespace centui
{

struct button_options
{
    bool                  enabled = true;
    std::function<void()> on_click;
};

template <typename... styles>
class button : public element
{
public:
    static constexpr element_kind widget_kind = element_kind::button;
    explicit button(std::string value, button_options options = {})
        : element(make_content(std::move(value), std::move(options)))
    {
    }

    void set_enabled(bool enabled)
    {
        auto& state = interaction();
        if(state.enabled == enabled)
        {
            return;
        }
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

    void set_text(std::string value)
    {
        set_text_value(std::move(value));
    }

    void value(std::string value)
    {
        set_text(std::move(value));
    }
    [[nodiscard]] std::string_view value() const
    {
        return std::get<details::button_content>(details::element_access::get(*this).data).value;
    }

    void set_on_click(std::function<void()> callback)
    {
        auto& state = interaction();
        if(!state.on_click && !callback)
        {
            return;
        }
        auto replacement = callback ? std::make_shared<std::function<void()>>(std::move(callback)) : nullptr;
        state.on_click.swap(replacement);
        changed({.interaction = true});
    }

private:
    friend class element;
    button(details::handle_tag, const element& handle)
        : element(handle)
    {
    }

    static details::element_content make_content(std::string value, button_options options)
    {
        auto interaction = std::make_shared<details::button_interaction_state>();
        interaction->enabled = options.enabled;
        if(options.on_click)
        {
            interaction->on_click = std::make_shared<std::function<void()>>(std::move(options.on_click));
        }
        return details::element_content{
            .data = details::button_content{
                .value = std::move(value),
                .interaction = std::move(interaction)},
            .style = style::resolve<styles...>(),
        };
    }

    [[nodiscard]] details::button_interaction_state& interaction()
    {
        auto& tree = details::element_access::get(*this);
        return *std::get<details::button_content>(tree.data).interaction;
    }

    [[nodiscard]] const details::button_interaction_state& interaction() const
    {
        const auto& tree = details::element_access::get(*this);
        return *std::get<details::button_content>(tree.data).interaction;
    }
};

} // namespace centui

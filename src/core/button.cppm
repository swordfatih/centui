module;

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

export module centui.button;

import centui.element;
import centui.model;
import centui.style;
import centui.text;

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
        : element(make_surface(std::move(options)))
    {
        centui::text<> label{std::move(value)};
        details::element_access::get(label).inherit_text_style = true;
        compose_part(label);
    }

    void set_enabled(bool enabled)
    {
        set_enabled_state(enabled);
    }

    [[nodiscard]] bool enabled() const
    {
        return enabled_state();
    }

    void set_text(std::string value)
    {
        auto label = caption();
        if(label.value() == value)
        {
            return;
        }
        label.set_text(std::move(value));
        changed({.text = true, .layout = true, .paint = true});
    }

    void value(std::string value)
    {
        set_text(std::move(value));
    }

    [[nodiscard]] std::string_view value() const
    {
        return caption().value();
    }

    void set_on_click(std::function<void()> callback)
    {
        set_activation_handler(std::move(callback));
    }

private:
    friend class element;
    button(details::handle_tag, const element& handle) : element(handle)
    {
    }

    [[nodiscard]] centui::text<> caption() const
    {
        return content_part().template as<centui::text<>>();
    }

    static details::element_content make_surface(button_options options)
    {
        details::interaction_capability interaction{.enabled = options.enabled};
        if(options.on_click)
        {
            interaction.activate = std::make_shared<std::function<void()>>(std::move(options.on_click));
        }
        return details::element_content{
            .data = container_data{},
            .style = centui::style::resolve<styles...>(),
            .kind = element_kind::button,
            .interaction = std::move(interaction),
        };
    }
};

} // namespace centui

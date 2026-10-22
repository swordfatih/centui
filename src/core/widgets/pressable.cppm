module;

#include <functional>
#include <initializer_list>
#include <span>
#include <utility>

export module centui.widgets.pressable;
export import centui.widgets.container;
import centui.widgets.callback;

export namespace centui
{
struct pressable_options
{
    bool enabled = true;
    std::function<void()> on_click{};
};

class pressable_widget : public container_widget
{
public:
    explicit pressable_widget(centui::style appearance = {}, pressable_options options = {})
        : container_widget(std::move(appearance))
    {
        set_interactive();
        set_focusable();
        set_enabled(options.enabled);
        set_on_click(std::move(options.on_click));
    }
    explicit pressable_widget(handle<> content, centui::style appearance = {}, pressable_options options = {})
        : pressable_widget(std::move(appearance), std::move(options))
    {
        append(content);
    }
    void set_on_click(std::function<void()> callback)
    {
        check_mutation();
        _on_click.set(std::move(callback));
    }

protected:
    virtual void activate()
    {
        _on_click();
    }

    bool on_event(const input_event& event) override
    {
        if(event.type == input_event_type::key_pressed && (event.code == key::space || event.code == key::enter))
        {
            if(!event.repeat && effectively_enabled())
            {
                activate();
            }
            return true;
        }

        switch(event.type)
        {
        case input_event_type::pointer_pressed:
        {
            _armed = event.button == mouse_button::primary;
            return _armed;
        }
        case input_event_type::pointer_released:
        {
            const auto box = bounds();
            const bool clicked = std::exchange(_armed, false) &&
                                 event.button == mouse_button::primary && effectively_enabled() && hovered() &&
                                 event.x >= box.x && event.y >= box.y &&
                                 event.x < box.x + box.width && event.y < box.y + box.height;
            if(clicked)
            {
                activate();
            }
            return event.button == mouse_button::primary;
        }
        case input_event_type::pointer_cancelled:
        {
            _armed = false;
            return true;
        }
        default:
        {
            return false;
        }
        }
    }

private:
    details::callback<> _on_click;
    bool _armed = false;
};

template <typename... Styles>
class pressable : public handle<pressable_widget>
{
public:
    pressable() : pressable(std::initializer_list<handle<>>{})
    {
    }
    explicit pressable(handle<> content, pressable_options options = {},
                       centui::style appearance = centui::style::resolve<Styles...>())
        : handle<pressable_widget>(make_widget<pressable_widget>(
              std::move(content), std::move(appearance), std::move(options)))
    {
    }
    explicit pressable(std::initializer_list<handle<>> children, pressable_options options = {},
                       centui::style appearance = centui::style::resolve<Styles...>())
        : handle<pressable_widget>(make_widget<pressable_widget>(std::move(appearance), std::move(options)))
    {
        this->get().append(std::span{children.begin(), children.size()});
    }
    void set_enabled(bool value)
    {
        this->get().set_enabled(value);
    }
    [[nodiscard]] bool enabled() const
    {
        return this->get().enabled();
    }
    void set_on_click(std::function<void()> callback)
    {
        this->get().set_on_click(std::move(callback));
    }
};
}

module;

#include <functional>
#include <string>
#include <string_view>
#include <utility>

export module centui.widgets.button;
export import centui.widgets.pressable;
export import centui.widgets.text;

export namespace centui
{
using button_options = pressable_options;

class button_widget : public pressable_widget
{
public:
    explicit button_widget(std::string value, centui::style appearance = {}, button_options options = {})
        : pressable_widget(std::move(appearance), std::move(options)),
          _text(make_widget<text_widget>(std::move(value), centui::style{}, true))
    {
        set_clip_children(true);
        append(_text);
    }
    void set_text(std::string value)
    {
        check_mutation();
        _text->set_text(std::move(value));
    }
    [[nodiscard]] std::string_view value() const noexcept
    {
        return _text->value();
    }

private:
    handle<text_widget> _text;
};

template <typename... Styles>
class button : public handle<button_widget>
{
public:
    explicit button(std::string value, button_options options = {},
                    centui::style appearance = centui::style::resolve<Styles...>())
        : handle<button_widget>(make_widget<button_widget>(std::move(value), std::move(appearance), std::move(options)))
    {
    }
    void set_text(std::string value)
    {
        this->get().set_text(std::move(value));
    }
    void value(std::string value)
    {
        set_text(std::move(value));
    }
    [[nodiscard]] std::string_view value() const
    {
        return this->get().value();
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

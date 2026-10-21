module;

#include <initializer_list>
#include <span>
#include <utility>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <functional>

export module centui.widgets;
export import centui.tree;

export namespace centui
{
class container_widget : public widget
{
public:
    container_widget(centui::style value, std::span<const handle<>> children) : widget(std::move(value))
    {
        attach_children(children);
    }
};

template <typename... Styles>
class container : public handle<container_widget>
{
public:
    container() : container(std::initializer_list<handle<>>{})
    {
    }
    // List initialization intentionally nests a single container of the same type.
    explicit container(std::initializer_list<handle<>> children)
        : handle<container_widget>(make_widget<container_widget>(
              centui::style::resolve<Styles...>(), std::span{children.begin(), children.size()}))
    {
    }
};
}

export namespace centui
{
class text_widget : public widget
{
public:
    explicit text_widget(std::string value, centui::style appearance = {})
        : widget(std::move(appearance)), _value(std::move(value))
    {
    }
    void set_text(std::string value)
    {
        check_mutation();
        if(_value == value)
        {
            return;
        }
        _value.swap(value);
        _cached.reset();
        invalidate_layout();
    }
    [[nodiscard]] std::string_view value() const noexcept
    {
        return _value;
    }

protected:
    size measure(const measure_context& context) override
    {
        const auto width = context.width.mode == constraint_mode::unconstrained ?
            std::numeric_limits<float>::infinity() : context.width.value;
        const auto& layout = prepared(context.text, width);
        return {layout.bounds.width, layout.bounds.height};
    }
    void paint(painter& output) const override
    {
        (void)prepared(output.fonts(), output.bounds().width);
        output.text(_cached);
    }

private:
    std::string _value;
    mutable std::shared_ptr<const text_layout> _cached;
    mutable std::shared_ptr<const void> _font_identity;
    mutable float _font_size = 0;
    mutable float _width = 0;

    const text_layout& prepared(text_service& fonts, float width) const
    {
        auto identity = fonts.font_identity(style().font);
        if(!_cached || identity != _font_identity || _font_size != style().font_size || _width != width)
        {
            auto candidate = fonts.prepare(_value, style().font, style().font_size, width);
            _cached = std::move(candidate);
            _font_identity = std::move(identity);
            _font_size = style().font_size;
            _width = width;
        }
        return *_cached;
    }
};

template <typename... Styles>
class text : public handle<text_widget>
{
public:
    explicit text(std::string value)
        : handle<text_widget>(make_widget<text_widget>(std::move(value), centui::style::resolve<Styles...>()))
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
};
}

export namespace centui
{
struct button_options
{
    bool enabled = true;
    std::function<void()> on_click;
};

class button_widget : public text_widget
{
public:
    button_widget(std::string value, centui::style appearance, button_options options)
        : text_widget(std::move(value), std::move(appearance))
    {
        set_interactive();
        set_enabled(options.enabled);
        set_on_click(std::move(options.on_click));
    }
    void set_on_click(std::function<void()> callback)
    {
        check_mutation();
        auto replacement = callback ? std::make_shared<std::function<void()>>(std::move(callback)) : nullptr;
        _on_click = std::move(replacement);
    }

protected:
    void on_event(const input_event& event) override
    {
        switch(event.type)
        {
        case input_event_type::pointer_pressed:
        {
            _armed = event.button == mouse_button::primary;
            break;
        }
        case input_event_type::pointer_released:
        {
            const auto box = bounds();
            const bool activate = std::exchange(_armed, false) &&
                event.button == mouse_button::primary && enabled() &&
                event.x >= box.x && event.y >= box.y &&
                event.x < box.x + box.width && event.y < box.y + box.height;
            if(activate)
            {
                // Retain the registration during self-replacement and preserve mutable captures.
                auto callback = _on_click;
                if(callback)
                {
                    (*callback)();
                }
            }
            break;
        }
        case input_event_type::pointer_cancelled:
        {
            _armed = false;
            break;
        }
        default:
        {
            break;
        }
        }
    }

private:
    std::shared_ptr<std::function<void()>> _on_click;
    bool _armed = false;
};

template <typename... Styles>
class button : public handle<button_widget>
{
public:
    explicit button(std::string value, button_options options = {})
        : handle<button_widget>(make_widget<button_widget>(std::move(value), centui::style::resolve<Styles...>(), std::move(options)))
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

module;

#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

export module centui.widgets;
export import centui.tree;

export namespace centui
{
class container_widget : public widget
{
public:
    explicit container_widget(centui::style value = {}, std::span<const handle<>> children = {}) : widget(std::move(value))
    {
        append(children);
    }
};

template <typename... Styles>
class container : public handle<container_widget>
{
public:
    container() : container(std::initializer_list<handle<>>{})
    {
    }
    explicit container(centui::style appearance)
        : handle<container_widget>(make_widget<container_widget>(std::move(appearance)))
    {
    }
    // List initialization intentionally nests a single container of the same type.
    explicit container(std::initializer_list<handle<>> children,
                       centui::style                   appearance = centui::style::resolve<Styles...>())
        : handle<container_widget>(make_widget<container_widget>(
              std::move(appearance), std::span{children.begin(), children.size()}))
    {
    }
};
} // namespace centui

export namespace centui
{
namespace details
{
// Shared text presentation for labels and text buttons; no tree or input behavior.
class text_content
{
public:
    explicit text_content(std::string value) : _value(std::move(value))
    {
    }
    bool set_text(std::string value)
    {
        if(_value == value)
        {
            return false;
        }
        _value.swap(value);
        _cached.reset();
        return true;
    }
    [[nodiscard]] std::string_view value() const noexcept
    {
        return _value;
    }

    size measure(const measure_context& context, const centui::style& appearance) const
    {
        const auto  width = context.width.mode == constraint_mode::unconstrained ? std::numeric_limits<float>::infinity() : context.width.value;
        const auto& layout = prepared(context.text, appearance, width);
        return {layout.bounds.width, layout.bounds.height};
    }
    void paint(painter& output, const centui::style& appearance) const
    {
        (void)prepared(output.fonts(), appearance, output.bounds().width);
        output.text(_cached);
    }

private:
    std::string                                _value;
    mutable std::shared_ptr<const text_layout> _cached;
    mutable std::shared_ptr<const void>        _font_identity;
    mutable float                              _font_size = 0;
    mutable float                              _width = 0;

    const text_layout& prepared(text_service& fonts, const centui::style& appearance, float width) const
    {
        auto identity = fonts.font_identity(appearance.font);
        if(!_cached || identity != _font_identity || _font_size != appearance.font_size || _width != width)
        {
            auto candidate = fonts.prepare(_value, appearance.font, appearance.font_size, width);
            _cached = std::move(candidate);
            _font_identity = std::move(identity);
            _font_size = appearance.font_size;
            _width = width;
        }
        return *_cached;
    }
};
} // namespace details

class text_widget : public widget
{
public:
    explicit text_widget(std::string value, centui::style appearance = {})
        : widget(std::move(appearance)), _text(std::move(value))
    {
    }
    void set_text(std::string value)
    {
        check_mutation();
        if(_text.set_text(std::move(value)))
        {
            invalidate_layout();
        }
    }
    [[nodiscard]] std::string_view value() const noexcept
    {
        return _text.value();
    }

protected:
    size measure(const measure_context& context) override
    {
        return _text.measure(context, style());
    }
    void paint(painter& output) const override
    {
        _text.paint(output, style());
    }

private:
    details::text_content _text;
};

template <typename... Styles>
class text : public handle<text_widget>
{
public:
    explicit text(std::string value, centui::style appearance = centui::style::resolve<Styles...>())
        : handle<text_widget>(make_widget<text_widget>(std::move(value), std::move(appearance)))
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
} // namespace centui

export namespace centui
{
struct button_options
{
    bool                  enabled = true;
    std::function<void()> on_click;
};

class button_widget : public widget
{
public:
    explicit button_widget(centui::style appearance = {}, button_options options = {})
        : widget(std::move(appearance))
    {
        set_interactive();
        set_focusable();
        set_enabled(options.enabled);
        set_on_click(std::move(options.on_click));
    }
    explicit button_widget(handle<> content, centui::style appearance = {}, button_options options = {})
        : button_widget(std::move(appearance), std::move(options))
    {
        append(content);
    }
    void set_on_click(std::function<void()> callback)
    {
        check_mutation();
        auto replacement = callback ? std::make_shared<std::function<void()>>(std::move(callback)) : nullptr;
        _on_click = std::move(replacement);
    }

protected:
    virtual void activate()
    {
        // Retain the registration during self-replacement and preserve mutable captures.
        auto callback = _on_click;
        if(callback)
        {
            (*callback)();
        }
    }

    bool on_event(const input_event& event) override
    {
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
    std::shared_ptr<std::function<void()>> _on_click;
    bool                                   _armed = false;
};

// Text is one presentation of a button. Content buttons use ordinary children.
class text_button_widget : public button_widget
{
public:
    explicit text_button_widget(std::string value, centui::style appearance = {}, button_options options = {})
        : button_widget(std::move(appearance), std::move(options)), _text(std::move(value))
    {
    }
    void set_text(std::string value)
    {
        check_mutation();
        if(_text.set_text(std::move(value)))
        {
            invalidate_layout();
        }
    }
    [[nodiscard]] std::string_view value() const noexcept
    {
        return _text.value();
    }

protected:
    size measure(const measure_context& context) override
    {
        return _text.measure(context, style());
    }
    void paint(painter& output) const override
    {
        _text.paint(output, style());
    }

private:
    details::text_content _text;
};

template <typename... Styles>
class button : public handle<text_button_widget>
{
public:
    explicit button(std::string value, button_options options = {},
                    centui::style appearance = centui::style::resolve<Styles...>())
        : handle<text_button_widget>(make_widget<text_button_widget>(std::move(value), std::move(appearance), std::move(options)))
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
} // namespace centui

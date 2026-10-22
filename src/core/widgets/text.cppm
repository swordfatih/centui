module;

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

export module centui.widgets.text;
export import centui.tree;

export namespace centui
{
class text_widget : public widget
{
public:
    explicit text_widget(std::string value, centui::style appearance = {}, bool inherit_text_style = false)
        : widget(std::move(appearance)), _value(std::move(value)), _inherit_text_style(inherit_text_style)
    {
    }
    void set_text(std::string value)
    {
        check_mutation();
        if(_value != value)
        {
            _value.swap(value);
            _cached.reset();
            invalidate_layout();
        }
    }
    [[nodiscard]] std::string_view value() const noexcept
    {
        return _value;
    }

protected:
    size measure(const measure_context& context) override
    {
        const auto width = context.width.mode == constraint_mode::unconstrained
                               ? std::numeric_limits<float>::infinity() : context.width.value;
        const auto& layout = prepared(context.text, text_style(), width);
        return {layout.bounds.width, layout.bounds.height};
    }
    void paint(painter& output) const override
    {
        const auto appearance = text_style();
        (void)prepared(output.fonts(), appearance, output.bounds().width);
        auto color = output.foreground();
        if(_inherit_text_style)
        {
            if(auto owner = parent(); owner.valid())
            {
                const auto state = !owner->effectively_enabled() ? details::visual_state::disabled
                                 : owner->pressed() ? details::visual_state::pressed
                                 : owner->hovered() ? details::visual_state::hovered
                                 : owner->focused() ? details::visual_state::focused
                                                    : details::visual_state::normal;
                color = details::state_color(appearance.text_color, state);
            }
        }
        output.text(_cached, output.bounds(), color, appearance.text_align);
    }

private:
    std::string _value;
    bool _inherit_text_style = false;
    mutable std::shared_ptr<const text_layout> _cached;
    mutable std::shared_ptr<const void> _font_identity;
    mutable float _font_size = 0;
    mutable float _width = 0;

    centui::style text_style() const
    {
        if(_inherit_text_style)
        {
            if(auto owner = parent(); owner.valid())
            {
                return owner->resolved_style();
            }
        }
        return resolved_style();
    }

    const text_layout& prepared(text_service& fonts, const centui::style& appearance, float width) const
    {
        auto identity = fonts.font_identity(appearance.font);
        if(!_cached || identity != _font_identity || _font_size != appearance.font_size || _width != width ||
           _cached->raster_scale != fonts.scale())
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
}

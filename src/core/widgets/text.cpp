#include <centui/widgets/text.hpp>

namespace centui
{

text_widget::text_widget(std::string value, centui::style appearance, bool inherit_text_style)
    : widget(std::move(appearance)),
      m_value(std::move(value)),
      m_inherit_text_style(inherit_text_style)
{
}

void text_widget::set_text(std::string value)
{
    check_mutation();

    if(m_value != value)
    {
        m_value.swap(value);
        m_cached.reset();
        invalidate_layout();
    }
}

std::string_view text_widget::value() const noexcept
{
    return m_value;
}

size text_widget::measure(const measure_context& context)
{
    const auto width = context.width.mode == constraint_mode::unconstrained ? std::numeric_limits<float>::infinity()
                                                                            : context.width.value;
    const auto& layout = prepared(context.text, text_style(), width);

    return {layout.bounds.width, layout.bounds.height};
}

void text_widget::paint(painter& output) const
{
    const auto appearance = text_style();
    (void)prepared(output.fonts(), appearance, output.bounds().width);
    auto color = output.foreground();

    if(m_inherit_text_style)
    {
        if(auto owner = parent(); owner.valid())
        {
            const auto state = !owner->effectively_enabled() ? details::visual_state::disabled
                               : owner->pressed()            ? details::visual_state::pressed
                               : owner->hovered()            ? details::visual_state::hovered
                               : owner->focused()            ? details::visual_state::focused
                                                             : details::visual_state::normal;
            color = details::state_color(appearance.text_color, state);
        }
    }

    output.text(m_cached, output.bounds(), color, appearance.text_align);
}

centui::style text_widget::text_style() const
{
    if(m_inherit_text_style)
    {
        if(auto owner = parent(); owner.valid())
        {
            return owner->resolved_style();
        }
    }

    return resolved_style();
}

const text_layout& text_widget::prepared(text_service& fonts, const centui::style& appearance, float width) const
{
    auto identity = fonts.font_identity(appearance.font);

    if(!m_cached || identity != m_font_identity || m_font_size != appearance.font_size || m_width != width ||
       m_cached->raster_scale != fonts.scale())
    {
        auto candidate = fonts.prepare(m_value, appearance.font, appearance.font_size, width);
        m_cached = std::move(candidate);
        m_font_identity = std::move(identity);
        m_font_size = appearance.font_size;
        m_width = width;
    }

    return *m_cached;
}

} // namespace centui

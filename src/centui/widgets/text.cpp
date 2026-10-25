#include <centui/widgets/text.hpp>

namespace centui
{

text_widget::text_widget(std::string value, centui::style_source appearance, bool inherit_text_style)
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
        m_cache.reset();
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

baseline_function text_widget::baseline()
{
    return [this](size content)
    {
        return prepared(fonts(), text_style(), content.width).ascender;
    };
}

void text_widget::paint(painter& output) const
{
    const auto appearance = text_style();
    (void)prepared(output.fonts(), appearance, output.bounds().width);
    output.text(
        m_cache.rasterize(output.fonts()),
        output.bounds(),
        output.resolve(appearance.text_color),
        appearance.text_align
    );
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
    return *m_cache.layout(fonts, m_value, appearance.font, appearance.font_size, width);
}

} // namespace centui

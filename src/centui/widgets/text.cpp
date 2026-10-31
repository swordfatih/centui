#include <centui/widgets/text.hpp>

namespace centui
{

text_widget::text_widget(std::string value, centui::style_source appearance, bool inherit_text_style)
    : widget(std::move(appearance)),
      m_value(std::move(value)),
      m_inherit_text_style(inherit_text_style)
{
    centypo::validate_text(m_value);
}

void text_widget::set_text(std::string value)
{
    check_mutation();
    centypo::validate_text(value);

    if(m_value != value)
    {
        m_value.swap(value);
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
    const auto layout = prepared(context.typography, width);

    return {layout.bounds().width, layout.bounds().height};
}

baseline_function text_widget::baseline()
{
    return [this](size content)
    {
        return prepared(typography(), content.width).baseline();
    };
}

void text_widget::on_geometry()
{
    m_layout = prepared(typography(), content_bounds().width);
}

void text_widget::paint(painter& output) const
{
    const auto appearance = text_style();
    output.text(
        output.typography().rasterize(m_layout, output.scale()),
        output.bounds(),
        output.resolve(appearance.text_color)
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

centypo::layout text_widget::prepared(const centypo::engine& engine, float width) const
{
    const auto appearance = text_style();
    return engine.layout(
        m_value,
        {.size = appearance.font_size,
         .width = width,
         .font = appearance.font.value_or(centypo::font{}),
         .paragraph = appearance.paragraph.value_or(centypo::text_options{}),
         .alignment = appearance.text_align}
    );
}

} // namespace centui

#include <centui/widgets/button.hpp>

namespace centui
{

button_widget::button_widget(std::string value, button_options options, centui::style_source appearance)
    : pressable_widget(std::move(options), std::move(appearance)),
      m_text(make_widget<text_widget>(std::move(value), centui::style_source{}, true))
{
    set_clip_children(true);
    append(m_text);
}

void button_widget::set_text(std::string value)
{
    check_mutation();
    m_text->set_text(std::move(value));
}

std::string_view button_widget::value() const noexcept
{
    return m_text->value();
}

} // namespace centui

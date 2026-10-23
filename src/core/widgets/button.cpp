#include <centui/widgets/button.hpp>

namespace centui
{

button_widget::button_widget(std::string value, centui::style appearance, button_options options)
    : pressable_widget(std::move(appearance), std::move(options)),
      m_text(make_widget<text_widget>(std::move(value), centui::style{}, true))
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

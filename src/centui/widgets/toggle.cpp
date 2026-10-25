#include <centui/widgets/toggle.hpp>

namespace centui
{

toggle_widget::toggle_widget(std::string label, toggle_options options, centui::style appearance)
    : button_widget(std::move(label), {}, std::move(appearance)),
      m_checked(options.checked)
{
    set_on_change(std::move(options.on_change));
}

bool toggle_widget::checked() const noexcept
{
    return m_checked;
}

void toggle_widget::set_checked(bool value)
{
    check_mutation();

    if(m_checked != value)
    {
        m_checked = value;
        invalidate_paint();
    }
}

void toggle_widget::set_on_change(std::function<void(bool)> callback)
{
    check_mutation();
    m_on_change.set(std::move(callback));
}

void toggle_widget::activate()
{
    set_checked(!m_checked);
    m_on_change(m_checked);
    button_widget::activate();
}

void toggle_widget::paint(painter& output) const
{
    if(m_checked)
    {
        auto line = output.bounds();
        line.y += std::max(0.0f, line.height - 2);
        line.height = std::min(2.0f, line.height);
        output.rectangle(line, output.foreground());
    }
}

} // namespace centui

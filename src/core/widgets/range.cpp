#include <centui/widgets/range.hpp>

namespace centui
{

range_widget::range_widget(range_options options, centui::style appearance)
    : widget(std::move(appearance))
{
    set_range(options.minimum, options.maximum, options.step);
    set_value(options.value);
    set_on_change(std::move(options.on_change));
    set_interactive();
    set_focusable();
}

void range_widget::set_range(float minimum, float maximum, float step)
{
    check_mutation();

    if(!std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(step) || maximum <= minimum ||
       !std::isfinite(maximum - minimum) || step <= 0)
    {
        throw std::invalid_argument("Range requires finite increasing bounds and a positive step");
    }

    m_minimum = minimum;
    m_maximum = maximum;
    m_step = step;
    set_value(m_value);
    invalidate_paint();
}

void range_widget::set_value(float value)
{
    check_mutation();

    if(!std::isfinite(value))
    {
        throw std::invalid_argument("Range value must be finite");
    }

    const auto bounded = std::clamp(value, m_minimum, m_maximum);
    const auto steps = std::round((static_cast<double>(bounded) - m_minimum) / m_step);
    m_value = static_cast<float>(
        std::clamp(m_minimum + steps * m_step, static_cast<double>(m_minimum), static_cast<double>(m_maximum))
    );

    if(bounded == m_maximum)
    {
        m_value = m_maximum;
    }

    invalidate_paint();
}

float range_widget::value() const noexcept
{
    return m_value;
}

void range_widget::set_on_change(std::function<void(float)> callback)
{
    check_mutation();
    m_on_change.set(std::move(callback));
}

size range_widget::measure(const measure_context&)
{
    return {160, 24};
}

void range_widget::paint(painter& output) const
{
    const auto box = output.bounds();
    const auto thumb = std::min(12.0f, box.width);
    const auto x = box.x + (box.width - thumb) * (m_value - m_minimum) / (m_maximum - m_minimum);
    output.rectangle({box.x, box.y + box.height / 2 - 1, box.width, 2}, output.foreground());
    output.rectangle({x, box.y, thumb, box.height}, output.foreground());
}

bool range_widget::on_event(const input_event& event)
{
    if(event.type == input_event_type::pointer_cancelled)
    {
        m_dragging = false;

        return true;
    }

    if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
    {
        m_dragging = true;
    }

    if(m_dragging &&
       (event.type == input_event_type::pointer_pressed || event.type == input_event_type::pointer_moved ||
        event.type == input_event_type::pointer_released))
    {
        const auto box = content_bounds();
        const auto thumb = std::min(12.0f, box.width);
        const auto fraction =
            box.width > thumb ? std::clamp((event.x - box.x - thumb / 2) / (box.width - thumb), 0.0f, 1.0f) : 0.0f;
        change(std::lerp(m_minimum, m_maximum, fraction));

        if(event.type == input_event_type::pointer_released)
        {
            m_dragging = false;
        }

        return true;
    }

    if(event.type != input_event_type::key_pressed)
    {
        return false;
    }

    switch(event.code)
    {
    case key::left:
    case key::down:
    {
        change(std::max(m_minimum, m_value - m_step));

        return true;
    }

    case key::right:
    case key::up:
    {
        change(std::min(m_maximum, m_value + m_step));

        return true;
    }

    case key::home:
    {
        change(m_minimum);

        return true;
    }

    case key::end:
    {
        change(m_maximum);

        return true;
    }

    default:
    {
        return false;
    }
    }
}

void range_widget::change(float value)
{
    const auto previous = m_value;
    set_value(value);

    if(previous != m_value)
    {
        m_on_change(m_value);
    }
}

} // namespace centui

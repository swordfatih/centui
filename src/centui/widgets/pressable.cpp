#include <centui/widgets/pressable.hpp>

namespace centui
{

pressable_widget::pressable_widget(pressable_options options, centui::style_source appearance)
    : container_widget(std::move(appearance))
{
    set_interactive();
    set_focusable();
    set_enabled(options.enabled);
    set_on_click(std::move(options.on_click));
}

pressable_widget::pressable_widget(handle<> content, pressable_options options, centui::style_source appearance)
    : pressable_widget(std::move(options), std::move(appearance))
{
    append(content);
}

void pressable_widget::set_on_click(centui::callback<> callback)
{
    check_mutation();
    m_on_click.set(std::move(callback));
}

void pressable_widget::activate()
{
    m_on_click(*this);
}

bool pressable_widget::on_event(const input_event& event)
{
    if(event.type == input_event_type::key_pressed && (event.code == key::space || event.code == key::enter))
    {
        if(!event.repeat && effectively_enabled())
        {
            activate();
        }

        return true;
    }

    switch(event.type)
    {
    case input_event_type::pointer_pressed:
    {
        m_armed = event.button == mouse_button::primary;

        return m_armed;
    }

    case input_event_type::pointer_released:
    {
        const auto box = bounds();
        const bool clicked = std::exchange(m_armed, false) && event.button == mouse_button::primary &&
                             effectively_enabled() && hovered() && event.x >= box.x && event.y >= box.y &&
                             event.x < box.x + box.width && event.y < box.y + box.height;

        if(clicked)
        {
            activate();
        }

        return event.button == mouse_button::primary;
    }

    case input_event_type::pointer_cancelled:
    {
        m_armed = false;

        return true;
    }

    default:
    {
        return false;
    }
    }
}

} // namespace centui

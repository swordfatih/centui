#include <centui/widgets/popup.hpp>

namespace centui
{

popup_widget::popup_widget(handle<> content, centui::style_source appearance)
    : container_widget(std::move(appearance))
{
    patch_style({
        .inset = edges::all(px(0)),
        .position = position::absolute,
        .width = percent(100),
        .height = percent(100),
        .visibility = visibility::collapsed,
    });
    append(content);
    set_interactive();
    set_focusable();
}

void popup_widget::open()
{
    check_mutation();

    if(m_open_requested)
    {
        return;
    }

    m_open_requested = true;
    set_modal_open(true);
}

void popup_widget::close()
{
    check_mutation();

    if(!m_open_requested)
    {
        return;
    }

    m_open_requested = false;
    set_modal_open(false);

    m_on_close(*this);
}

void popup_widget::set_on_close(centui::callback<> callback)
{
    check_mutation();
    m_on_close.set(std::move(callback));
}

bool popup_widget::on_event(const input_event& event)
{
    if(event.type == input_event_type::key_pressed && event.code == key::escape)
    {
        close();

        return true;
    }

    if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
    {
        for(const auto& child: children())
        {
            if(!child->effectively_visible())
            {
                continue;
            }
            const auto box = child->bounds();

            if(event.x >= box.x && event.y >= box.y && event.x < box.x + box.width && event.y < box.y + box.height)
            {
                return true;
            }
        }

        close();

        return true;
    }

    return event.type == input_event_type::wheel;
}

} // namespace centui

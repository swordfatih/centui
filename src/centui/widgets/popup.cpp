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
    set_focus_scope();
}

void popup_widget::open()
{
    check_mutation();

    if(style().visibility == visibility::visible)
    {
        return;
    }

    auto root = parent();

    while(root.valid() && root->parent().valid())
    {
        root = root->parent();
    }

    if(root.valid())
    {
        m_previous_focus = find_focus(root).weak();
    }

    m_focus_pending = true;
    patch_style({.visibility = visibility::visible});
}

void popup_widget::close()
{
    check_mutation();

    if(style().visibility != visibility::visible)
    {
        return;
    }

    m_focus_pending = false;
    patch_style({.visibility = visibility::collapsed});

    if(auto previous = m_previous_focus.lock(); previous.valid())
    {
        previous->request_focus();
    }

    m_on_close(*this);
}

void popup_widget::set_on_close(centui::callback<> callback)
{
    check_mutation();
    m_on_close.set(std::move(callback));
}

void popup_widget::on_layout()
{
    if(m_focus_pending && effectively_visible())
    {
        m_focus_pending = false;
        focus_next();
    }
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

handle<> popup_widget::find_focus(const handle<>& root)
{
    if(root->focused())
    {
        return root;
    }

    for(const auto& child: root->children())
    {
        if(auto result = find_focus(child); result.valid())
        {
            return result;
        }
    }

    return {};
}

} // namespace centui

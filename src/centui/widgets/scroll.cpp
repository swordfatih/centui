#include <centui/widgets/scroll.hpp>

namespace centui
{

scroll_widget::scroll_widget(centui::style_source appearance)
    : container_widget(std::move(appearance))
{
    patch_style({.overflow = overflow::scroll});
    set_interactive();
    set_focusable();
}

vec2 scroll_widget::offset() const noexcept
{
    return m_offset;
}

vec2 scroll_widget::limit() const noexcept
{
    return m_limit;
}

void scroll_widget::set_offset(vec2 value)
{
    check_mutation();

    if(!std::isfinite(value.x) || !std::isfinite(value.y))
    {
        throw std::invalid_argument("Scroll offset must be finite");
    }

    m_offset = {std::clamp(value.x, 0.0f, m_limit.x), std::clamp(value.y, 0.0f, m_limit.y)};
    set_child_offset({-m_offset.x, -m_offset.y});
}

void scroll_widget::on_layout()
{
    const auto area = content_bounds();
    vec2 extent{};

    for(const auto& child: children())
    {
        if(child->style().visibility == visibility::collapsed)
        {
            continue;
        }

        const auto box = child->bounds();
        extent.x = std::max(extent.x, box.x + box.width + m_offset.x - area.x);
        extent.y = std::max(extent.y, box.y + box.height + m_offset.y - area.y);
    }

    m_limit = {std::max(0.0f, extent.x - area.width), std::max(0.0f, extent.y - area.height)};
    set_offset(m_offset);
}

bool scroll_widget::on_event(const input_event& event)
{
    auto next = m_offset;

    if(event.type == input_event_type::wheel)
    {
        next.x += event.delta_x;
        next.y += event.delta_y;
    }
    else if(event.type == input_event_type::key_pressed && event.code == key::page_down)
    {
        next.y += content_bounds().height;
    }
    else if(event.type == input_event_type::key_pressed && event.code == key::page_up)
    {
        next.y -= content_bounds().height;
    }
    else
    {
        return false;
    }

    const auto previous = m_offset;
    set_offset(next);

    return m_offset != previous;
}

} // namespace centui

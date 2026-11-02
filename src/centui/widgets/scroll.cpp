#include <centui/widgets/scroll.hpp>

namespace centui
{

bool scroll_widget::can_scroll(vec2 delta) const
{
    return (delta.x < 0 && m_offset.x > 0) || (delta.x > 0 && m_offset.x < m_limit.x) ||
           (delta.y < 0 && m_offset.y > 0) || (delta.y > 0 && m_offset.y < m_limit.y);
}

namespace
{

float reveal_delta(float start, float length, float viewport_start, float viewport_length)
{
    const auto end = start + length;
    const auto viewport_end = viewport_start + viewport_length;
    if(viewport_length <= 0 || (start >= viewport_start && end <= viewport_end) ||
       (start <= viewport_start && end >= viewport_end))
    {
        return 0;
    }
    if(length <= viewport_length)
    {
        return start < viewport_start ? start - viewport_start : end - viewport_end;
    }
    return start > viewport_start ? start - viewport_start : end - viewport_end;
}

} // namespace

vec2 scroll_widget::reveal_descendant(rect target)
{
    const auto previous = offset();
    const auto area = content_bounds();
    if(area.width <= 0 || area.height <= 0)
    {
        return {};
    }
    set_offset(
        {previous.x + reveal_delta(target.x, target.width, area.x, area.width),
         previous.y + reveal_delta(target.y, target.height, area.y, area.height)}
    );
    const auto applied = offset();
    return {previous.x - applied.x, previous.y - applied.y};
}

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

void scroll_widget::set_scrollbars(scrollbar_options options)
{
    check_mutation();
    for(const auto value: {options.thickness, options.minimum_thumb, options.thumb_inset, options.corner_radius})
    {
        if(!std::isfinite(value) || value < 0)
        {
            throw std::invalid_argument("Scrollbar dimensions must be finite and nonnegative");
        }
    }
    if(m_scrollbar != options)
    {
        m_scrollbar = options;
        m_drag_axis = axis::none;
        invalidate_geometry();
    }
}

scrollbar_options scroll_widget::scrollbars() const noexcept
{
    return m_scrollbar;
}

void scroll_widget::on_geometry()
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
    if((m_drag_axis == axis::horizontal && m_limit.x == 0) || (m_drag_axis == axis::vertical && m_limit.y == 0))
    {
        m_drag_axis = axis::none;
    }
    set_offset(m_offset);
}

scroll_widget::scrollbar scroll_widget::bar(axis direction) const noexcept
{
    const auto thickness = m_scrollbar.thickness;
    const auto minimum_thumb = m_scrollbar.minimum_thumb;
    const auto area = content_bounds();
    const bool vertical = direction == axis::vertical;
    const auto limit = vertical ? m_limit.y : m_limit.x;
    const auto viewport = vertical ? area.height : area.width;
    if(limit <= 0 || area.width <= 0 || area.height <= 0)
    {
        return {};
    }

    const auto cross = std::min(thickness, vertical ? area.width : area.height);
    const auto other = (vertical ? m_limit.x : m_limit.y) > 0 ? std::min(thickness, viewport) : 0;
    const auto length = std::max(0.0f, viewport - other);
    const auto thumb_length = std::min(length, std::max(minimum_thumb, length * viewport / (viewport + limit)));
    const auto position = (length - thumb_length) * (vertical ? m_offset.y : m_offset.x) / limit;
    if(vertical)
    {
        return {{area.width - cross, 0, cross, length}, {area.width - cross, position, cross, thumb_length}};
    }
    return {{0, area.height - cross, length, cross}, {position, area.height - cross, thumb_length, cross}};
}

void scroll_widget::paint_overlay(painter& output) const
{
    auto track_color = output.foreground();
    auto thumb_color = track_color;
    track_color.a = static_cast<std::uint8_t>(track_color.a * 0.10f);
    thumb_color.a = static_cast<std::uint8_t>(thumb_color.a * (hovered() || m_drag_axis != axis::none ? 0.75f : 0.45f));

    for(const auto direction: {axis::horizontal, axis::vertical})
    {
        auto [track, thumb] = bar(direction);
        if(track.width <= 0 || track.height <= 0)
        {
            continue;
        }
        output.rectangle(track, track_color);
        const auto inset =
            std::min(m_scrollbar.thumb_inset, (direction == axis::vertical ? thumb.width : thumb.height) / 4);
        if(direction == axis::vertical)
        {
            thumb.x += inset;
            thumb.width -= 2 * inset;
        }
        else
        {
            thumb.y += inset;
            thumb.height -= 2 * inset;
        }
        output.rounded_rectangle(thumb, m_scrollbar.corner_radius, thumb_color);
    }
}

bool scroll_widget::hit_test_overlay(vec2 position) const
{
    return bar(axis::horizontal).track.contains(position) || bar(axis::vertical).track.contains(position);
}

bool scroll_widget::on_event(const input_event& event)
{
    if(event.type == input_event_type::pointer_cancelled)
    {
        m_drag_axis = axis::none;
        invalidate_paint();
        return true;
    }
    if(event.type == input_event_type::pointer_released && event.button == mouse_button::primary)
    {
        const bool dragging = m_drag_axis != axis::none;
        m_drag_axis = axis::none;
        invalidate_paint();
        return dragging;
    }

    const auto area = content_bounds();
    const vec2 pointer{event.x - area.x, event.y - area.y};
    if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
    {
        for(const auto direction: {axis::vertical, axis::horizontal})
        {
            const auto [track, thumb] = bar(direction);
            if(!track.contains(pointer))
            {
                continue;
            }
            const bool vertical = direction == axis::vertical;
            const auto point = vertical ? pointer.y : pointer.x;
            const auto start = vertical ? thumb.y : thumb.x;
            if(thumb.contains(pointer))
            {
                m_drag_axis = direction;
                m_drag_grab = (point - start) / (vertical ? thumb.height : thumb.width);
            }
            else
            {
                auto next = m_offset;
                auto& value = vertical ? next.y : next.x;
                value += (point < start ? -1 : 1) * (vertical ? area.height : area.width);
                set_offset(next);
            }
            invalidate_paint();
            return true;
        }
    }
    if(event.type == input_event_type::pointer_moved && m_drag_axis != axis::none)
    {
        const bool vertical = m_drag_axis == axis::vertical;
        const auto [track, thumb] = bar(m_drag_axis);
        const auto length = vertical ? thumb.height : thumb.width;
        const auto travel = (vertical ? track.height : track.width) - length;
        if(travel > 0)
        {
            const auto position = (vertical ? pointer.y - track.y : pointer.x - track.x) - m_drag_grab * length;
            auto next = m_offset;
            (vertical ? next.y : next.x) =
                std::clamp(position / travel, 0.0f, 1.0f) * (vertical ? m_limit.y : m_limit.x);
            set_offset(next);
        }
        return true;
    }

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

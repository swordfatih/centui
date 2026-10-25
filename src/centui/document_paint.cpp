#include "document_state.hpp"

namespace centui
{

void document::paint_tree(
    const widget& value,
    draw_list& output,
    rect clip,
    bool enabled,
    details::visual_state inherited_state
)
{
    const auto key = value.weak_from_this();
    if(!value.m_paint_boundary)
    {
        if(const auto found = m_data->commands.find(key); found != m_data->commands.end())
        {
            m_data->statistics.command_cache_bytes -= found->second.commands.size() * sizeof(draw_quad);
            m_data->commands.erase(found);
        }
    }

    if(value.m_paint_boundary)
    {
        const auto found = m_data->commands.find(key);
        if(found != m_data->commands.end())
        {
            if(found->second.revision == value.m_paint_revision)
            {
                output.commands
                    .insert(output.commands.end(), found->second.commands.begin(), found->second.commands.end());
                ++m_data->statistics.command_cache_hits;
                return;
            }
            m_data->statistics.command_cache_bytes -= found->second.commands.size() * sizeof(draw_quad);
            m_data->commands.erase(found);
        }
    }
    const auto begin = output.commands.size();
    ++m_data->statistics.paint_visits;
    paint_tree_uncached(value, output, clip, enabled, inherited_state);
    const auto bytes = (output.commands.size() - begin) * sizeof(draw_quad);
    if(value.m_paint_boundary && bytes > 0 && bytes <= m_data->command_budget &&
       m_data->statistics.command_cache_bytes <= m_data->command_budget - bytes)
    {
        data::command_entry entry{value.m_paint_revision, {output.commands.begin() + begin, output.commands.end()}};
        m_data->commands.emplace(key, std::move(entry));
        m_data->statistics.command_cache_bytes += bytes;
    }
}

void document::paint_tree_uncached(
    const widget& value,
    draw_list& output,
    rect clip,
    bool enabled,
    details::visual_state inherited_state
)
{
    if(value.m_style.visibility != visibility::visible || value.m_style.display == display::none)
    {
        return;
    }

    enabled = enabled && value.m_enabled;
    const auto state = !enabled               ? details::visual_state::disabled
                       : value.m_pressed      ? details::visual_state::pressed
                       : value.m_hovered      ? details::visual_state::hovered
                       : value.m_focus_active ? details::visual_state::focused
                       : !value.m_interactive ? inherited_state
                                              : details::visual_state::normal;
    if(value.m_style.display == display::contents)
    {
        for(const auto& child: value.m_children)
        {
            paint_tree(*child, output, clip, enabled, state);
        }
        return;
    }

    const auto appearance = value.resolved_style();
    painter background{output, m_data->fonts, m_data->drawings, value.m_bounds, clip, appearance, state};
    background.rounded_rectangle(
        background.bounds(),
        value.m_style.corner_radius,
        details::state_color(value.m_style.background_color, state)
    );
    const auto border = details::state_color(value.m_style.border_color, state);
    const auto widths = value.m_metrics.border;

    if(widths.top > 0 || widths.right > 0 || widths.bottom > 0 || widths.left > 0)
    {
        const auto box = background.bounds();
        const auto top = std::min(widths.top, box.height);
        const auto bottom = std::min(widths.bottom, box.height - top);
        const auto left = std::min(widths.left, box.width);
        const auto right = std::min(widths.right, box.width - left);
        if(value.m_style.corner_radius > 0)
        {
            background.box_border(box, value.m_style.corner_radius, {top, right, bottom, left}, border);
        }
        else
        {
            background.rectangle({box.x, box.y, box.width, top}, border);
            background.rectangle({box.x, box.y + box.height - bottom, box.width, bottom}, border);
            background.rectangle({box.x, box.y + top, left, box.height - top - bottom}, border);
            background.rectangle({box.x + box.width - right, box.y + top, right, box.height - top - bottom}, border);
        }
    }

    painter content{
        output,
        m_data->fonts,
        m_data->drawings,
        value.m_content,
        intersection(clip, value.m_content),
        appearance,
        state
    };
    value.paint(content);
    const auto child_clip = value.m_clip_children || value.m_style.overflow != overflow::visible
                                ? intersection(clip, value.m_content)
                                : clip;

    for(const auto& child: value.m_children)
    {
        paint_tree(*child, output, child_clip, enabled, state);
    }

    const auto indicator = value.m_style.focus;
    if(value.focus_visible() && indicator.width > 0)
    {
        auto box = value.focus_bounds();
        box.x = box.x - value.m_bounds.x + indicator.inset;
        box.y = box.y - value.m_bounds.y + indicator.inset;
        box.width = std::max(0.0f, box.width - 2 * indicator.inset);
        box.height = std::max(0.0f, box.height - 2 * indicator.inset);
        const auto width = std::min({indicator.width, box.width / 2, box.height / 2});
        background.box_border(
            box,
            std::max(0.0f, value.m_style.corner_radius - indicator.inset),
            {width, width, width, width},
            indicator.color
        );
    }
}

} // namespace centui

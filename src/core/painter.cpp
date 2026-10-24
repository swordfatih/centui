#include "paint_service.hpp"

#include <centui/painter.hpp>

namespace centui
{

namespace details
{

[[nodiscard]] centui::color state_color(const states_color& colors, visual_state state) noexcept
{
    switch(state)
    {
    case visual_state::disabled:
    {
        return colors.disabled;
    }

    case visual_state::pressed:
    {
        return colors.pressed;
    }

    case visual_state::hovered:
    {
        return colors.hover;
    }

    case visual_state::focused:
    {
        return colors.focused;
    }

    default:
    {
        return colors.normal;
    }
    }
}

} // namespace details

painter::painter(
    draw_list& output,
    text_service& fonts,
    details::paint_service& drawings,
    rect content,
    rect clip,
    const centui::style& style,
    details::visual_state state
)
    : m_output(output),
      m_fonts(fonts),
      m_drawings(drawings),
      m_bounds(content),
      m_clip(clip),
      m_style(style),
      m_state(state)
{
}

rect painter::bounds() const noexcept
{
    return {0, 0, m_bounds.width, m_bounds.height};
}

float painter::scale() const noexcept
{
    return m_output.viewport.scale;
}

text_service& painter::fonts() const noexcept
{
    return m_fonts;
}

centui::color painter::foreground() const noexcept
{
    return details::state_color(m_style.text_color, m_state);
}

centui::color painter::resolve(const states_color& colors) const noexcept
{
    return details::state_color(colors, m_state);
}

void painter::rectangle(rect bounds, centui::color color)
{
    bounds.x += m_bounds.x;
    bounds.y += m_bounds.y;
    const auto clip = intersection(
        intersection(bounds, m_clip),
        {0, 0, m_output.viewport.logical_width(), m_output.viewport.logical_height()}
    );

    if(color.a != 0 && clip.width > 0 && clip.height > 0)
    {
        m_output.commands.push_back({clip, {0, 0, 1, 1}, {}, color});
    }
}

void painter::text(std::shared_ptr<const text_layout> layout)
{
    text(std::move(layout), bounds(), details::state_color(m_style.text_color, m_state), m_style.text_align);
}

void painter::image(std::shared_ptr<const bitmap> value, rect bounds, centui::color tint, image_sampling sampling)
{
    bounds.x += m_bounds.x;
    bounds.y += m_bounds.y;
    if(value)
    {
        const image_command command{bounds, m_clip, value, tint, sampling};
        details::draw_image(
            command,
            {0, 0, m_output.viewport.logical_width(), m_output.viewport.logical_height()},
            [&](rect clipped, rect uv)
            {
                m_output.commands.push_back({clipped, uv, value, tint, sampling});
            }
        );
    }
}

void painter::text(std::string_view value)
{
    if(!value.empty())
    {
        text(m_fonts.prepare(value, m_style.font, m_style.font_size, std::max(0.0f, m_bounds.width)));
    }
}

void painter::text(
    std::shared_ptr<const text_layout> layout,
    rect bounds,
    centui::color color,
    text_alignment alignment
)
{
    bounds.x += m_bounds.x;
    bounds.y += m_bounds.y;
    const auto clip = intersection(
        intersection(bounds, m_clip),
        {0, 0, m_output.viewport.logical_width(), m_output.viewport.logical_height()}
    );

    if(layout && color.a != 0 && clip.width > 0 && clip.height > 0)
    {
        const text_command command{bounds, color, std::move(layout), alignment, clip};
        details::for_each_glyph(
            command,
            {0, 0, m_output.viewport.logical_width(), m_output.viewport.logical_height()},
            [&](const glyph_quad& glyph)
            {
                m_output.commands.push_back(
                    {glyph.bounds, glyph.uv, glyph.image, glyph.color, image_sampling::nearest}
                );
            }
        );
    }
}

rect painter::visible(vec2 offset) const
{
    auto clip = intersection(m_clip, {0, 0, m_output.viewport.logical_width(), m_output.viewport.logical_height()});
    clip.x -= m_bounds.x + offset.x;
    clip.y -= m_bounds.y + offset.y;
    return clip;
}

void painter::draw_tiles(const std::vector<details::painted_image>& tiles, vec2 offset, color tint)
{
    for(const auto& tile: tiles)
    {
        auto bounds = tile.bounds;
        auto clip = tile.clip;
        bounds.x += m_bounds.x + offset.x;
        bounds.y += m_bounds.y + offset.y;
        clip.x += m_bounds.x + offset.x;
        clip.y += m_bounds.y + offset.y;
        details::draw_image(
            {bounds, intersection(m_clip, clip), tile.image, tint, image_sampling::linear},
            {0, 0, m_output.viewport.logical_width(), m_output.viewport.logical_height()},
            [&](rect clipped, rect uv)
            {
                m_output.commands.push_back({clipped, uv, tile.image, tint, image_sampling::linear});
            }
        );
    }
}

void painter::fill(const path& shape, const brush& paint, vec2 offset)
{
    const auto* solid = std::get_if<color>(&paint);
    if(solid && solid->a == 0)
    {
        return;
    }
    draw_tiles(
        m_drawings.prepare(shape, solid ? brush{color{255, 255, 255, 255}} : paint, 0, scale(), visible(offset)),
        offset,
        solid ? *solid : color{255, 255, 255, 255}
    );
}

void painter::stroke(const path& shape, const brush& paint, float width, vec2 offset)
{
    if(width == 0)
    {
        return;
    }
    const auto* solid = std::get_if<color>(&paint);
    if(solid && solid->a == 0)
    {
        return;
    }
    draw_tiles(
        m_drawings.prepare(shape, solid ? brush{color{255, 255, 255, 255}} : paint, width, scale(), visible(offset)),
        offset,
        solid ? *solid : color{255, 255, 255, 255}
    );
}

void painter::rounded_rectangle(rect bounds, float radius, color tint)
{
    if(tint.a == 0)
    {
        return;
    }
    if(radius == 0)
    {
        rectangle(bounds, tint);
        return;
    }
    const vec2 offset{bounds.x, bounds.y};
    draw_tiles(m_drawings.box({bounds.width, bounds.height}, radius, {}, scale(), visible(offset)), offset, tint);
}

void painter::box_border(rect bounds, float radius, computed_edges widths, color tint)
{
    if(tint.a == 0)
    {
        return;
    }
    const vec2 offset{bounds.x, bounds.y};
    draw_tiles(m_drawings.box({bounds.width, bounds.height}, radius, widths, scale(), visible(offset)), offset, tint);
}

} // namespace centui

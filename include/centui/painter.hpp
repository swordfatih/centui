#pragma once

#include <centui/font.hpp>
#include <centui/layout.hpp>
#include <centui/paint.hpp>
#include <centui/style.hpp>

namespace centui
{

/// \brief Records drawing in local content coordinates within the inherited clip.
class painter
{
public:
    [[nodiscard]] rect bounds() const noexcept;

    [[nodiscard]] float scale() const noexcept;

    [[nodiscard]] text_service& fonts() const noexcept;

    [[nodiscard]] centui::color foreground() const noexcept;

    [[nodiscard]] centui::color resolve(const states_color& colors) const noexcept;

    void rectangle(rect bounds, centui::color color);

    void rounded_rectangle(rect bounds, float radius, centui::color color);

    /// \brief Fills a path in local content coordinates, using the nonzero winding rule.
    void fill(const path& shape, const brush& paint, vec2 offset = {});

    /// \brief Strokes a local path with round joins and caps; width is in logical units.
    void stroke(const path& shape, const brush& paint, float width, vec2 offset = {});

    void text(std::shared_ptr<const text_layout> layout);

    void image(
        std::shared_ptr<const bitmap> value,
        rect bounds,
        centui::color tint = {255, 255, 255, 255},
        image_sampling sampling = image_sampling::linear
    );

    void text(std::string_view value);

    void text(
        std::shared_ptr<const text_layout> layout,
        rect bounds,
        centui::color color,
        text_alignment alignment = text_alignment::left
    );

private:
    friend class document;
    painter(
        draw_list& output,
        text_service& fonts,
        details::paint_service& drawings,
        rect content,
        rect clip,
        const centui::style& style,
        details::visual_state state = details::visual_state::normal
    );

    void box_border(rect bounds, float radius, computed_edges widths, centui::color color);
    [[nodiscard]] rect visible(vec2 offset = {}) const;
    void draw_tiles(const std::vector<details::painted_image>& tiles, vec2 offset, centui::color tint);

    draw_list& m_output;
    text_service& m_fonts;
    details::paint_service& m_drawings;
    rect m_bounds;
    rect m_clip;
    const centui::style& m_style;
    details::visual_state m_state;
};

} // namespace centui

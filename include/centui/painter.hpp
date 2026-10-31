#pragma once

#include <centui/layout.hpp>
#include <centui/paint.hpp>
#include <centui/render.hpp>
#include <centui/style.hpp>

#include <centypo/engine.hpp>

namespace centui
{

/// \brief Records drawing in local content coordinates within the inherited clip.
class painter
{
public:
    [[nodiscard]] rect bounds() const noexcept;

    [[nodiscard]] float scale() const noexcept;

    [[nodiscard]] const centypo::engine& typography() const noexcept;

    [[nodiscard]] centui::color foreground() const noexcept;

    [[nodiscard]] centui::color resolve(const states_color& colors) const noexcept;

    void rectangle(rect bounds, centui::color color);

    void rounded_rectangle(rect bounds, float radius, centui::color color);

    /// \brief Fills a path in local content coordinates, using the nonzero winding rule.
    void fill(const path& shape, const brush& paint, vec2 offset = {});

    /// \brief Strokes a local path with round joins and caps; width is in logical units.
    void stroke(const path& shape, const brush& paint, float width, vec2 offset = {});

    /// Draws prepared geometry using its existing alignment.
    void text(centypo::layout layout);

    void image(
        std::shared_ptr<const bitmap> value,
        rect bounds,
        centui::color tint = {255, 255, 255, 255},
        image_sampling sampling = image_sampling::linear
    );

    void text(std::string_view value);

    void text(
        centypo::layout layout,
        rect bounds,
        centui::color color,
        bool clip_to_content = false
    );

private:
    friend class document;
    painter(
        draw_list& output,
        const centypo::engine& fonts,
        details::paint_service& drawings,
        rect content,
        rect clip,
        const centui::style& style,
        details::visual_state state = details::visual_state::normal,
        std::optional<rect> text_clip = {}
    );

    void box_border(rect bounds, float radius, computed_edges widths, centui::color color);
    [[nodiscard]] rect visible(vec2 offset = {}) const;
    void draw_tiles(const std::vector<details::painted_image>& tiles, vec2 offset, centui::color tint);

    draw_list& m_output;
    const centypo::engine& m_typography;
    details::paint_service& m_drawings;
    rect m_bounds;
    rect m_clip;
    rect m_text_clip;
    const centui::style& m_style;
    details::visual_state m_state;
};

} // namespace centui

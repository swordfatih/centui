#pragma once

#include <centypo/layout.hpp>

namespace centypo::details
{

class font_collection;

/// \brief Prepared glyphs, line metrics, and caret positions in logical units.
struct layout_data
{
    using line = text_line;

    std::vector<glyph> glyphs;

    rect bounds{};
    rect ink_bounds{};
    float ascender = 0;

    std::vector<line> lines;

    using caret = text_caret;

    float font_size = 0;
    float line_height = 0;
    float raster_scale = 1;
    bool rasterized = false;

    std::vector<caret> carets;

    using selection_cell = text_selection_cell;

    std::vector<selection_cell> selection_cells;

    // Exact native font ownership survives service destruction and cache eviction.
    std::shared_ptr<const details::font_collection> font_resource;
};

struct glyph_access
{
    static auto& face(glyph& value)
    {
        return value.m_face;
    }

    static const auto& face(const glyph& value)
    {
        return value.m_face;
    }
};

struct layout_access
{
    static const layout_data& get(const layout& value);

    static std::shared_ptr<const layout_data> share(const layout& value)
    {
        if(value.m_data)
        {
            return value.m_data;
        }

        static const auto empty = std::make_shared<const layout_data>();
        return empty;
    }

    static layout make(std::shared_ptr<const layout_data> value)
    {
        return layout{std::move(value)};
    }
};

rect caret_bounds(const layout_data&, text_position);
rect caret_bounds(const layout_data&, std::size_t);

text_position hit_text(const layout_data&, vec2);
text_position move_visual(const layout_data&, text_position, bool);
std::size_t text_hit(const layout_data&, vec2);

std::vector<rect> selection_bounds(const layout_data&, std::size_t, std::size_t);

} // namespace centypo::details

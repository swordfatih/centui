#include "font_face.hpp"
#include "layout_data.hpp"
#include "text_layout.hpp"

#include <stdexcept>

namespace centypo
{

glyph::glyph(std::uint32_t id, float x, float y, std::uint32_t cluster, std::shared_ptr<const glyph_bitmap> image)
    : id(id),
      x(x),
      y(y),
      cluster(cluster),
      image(std::move(image))
{
}

std::uintptr_t glyph::font_id() const noexcept
{
    return reinterpret_cast<std::uintptr_t>(m_face.get());
}

std::string glyph::family() const
{
    return m_face ? m_face->family() : std::string{};
}

glyph_bitmap::glyph_bitmap(int width, int height, int left, int top, std::vector<std::uint8_t> pixels)
    : bitmap{width, height, std::move(pixels), pixel_format::alpha},
      left(left),
      top(top)
{
}

const details::layout_data& details::layout_access::get(const layout& value)
{
    static const layout_data empty;

    return value.m_data ? *value.m_data : empty;
}

rect layout::bounds() const noexcept
{
    return details::layout_access::get(*this).bounds;
}

rect layout::ink_bounds() const noexcept
{
    return details::layout_access::get(*this).ink_bounds;
}

float layout::baseline() const noexcept
{
    return details::layout_access::get(*this).ascender;
}

float layout::font_size() const noexcept
{
    return details::layout_access::get(*this).font_size;
}

float layout::line_height() const noexcept
{
    return details::layout_access::get(*this).line_height;
}

float layout::raster_scale() const noexcept
{
    return details::layout_access::get(*this).raster_scale;
}

std::span<const glyph> layout::glyphs() const noexcept
{
    return details::layout_access::get(*this).glyphs;
}

std::span<const text_line> layout::lines() const noexcept
{
    return details::layout_access::get(*this).lines;
}

std::span<const text_caret> layout::carets() const noexcept
{
    return details::layout_access::get(*this).carets;
}

std::span<const text_selection_cell> layout::selection_cells() const noexcept
{
    return details::layout_access::get(*this).selection_cells;
}

rect layout::caret_bounds(text_position position) const
{
    return details::caret_bounds(details::layout_access::get(*this), position);
}

text_position layout::hit_test(vec2 point) const
{
    return details::hit_text(details::layout_access::get(*this), point);
}

text_position layout::move_visual(text_position position, bool forward) const
{
    return details::move_visual(details::layout_access::get(*this), position, forward);
}

std::vector<rect> layout::selection_bounds(std::size_t begin, std::size_t end) const
{
    return details::selection_bounds(details::layout_access::get(*this), begin, end);
}

layout layout::aligned(float width, text_alignment alignment) const
{
    if(std::isnan(width) || width < 0)
    {
        throw std::invalid_argument("Text width must be nonnegative");
    }

    auto next = std::make_shared<details::layout_data>(details::layout_access::get(*this));
    details::place_text(*next, width, alignment);

    return details::layout_access::make(std::move(next));
}

} // namespace centypo

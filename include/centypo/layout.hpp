#pragma once

#include <memory>
#include <span>
#include <string>
#include <utility>

#include <centypo/types.hpp>

namespace centypo
{
namespace details
{

class font_face;
struct glyph_access;
struct layout_data;
struct layout_access;

} // namespace details

class glyph
{
public:
    glyph(
        std::uint32_t id = 0,
        float x = 0,
        float y = 0,
        std::uint32_t cluster = 0,
        std::shared_ptr<const glyph_bitmap> image = {}
    );

    [[nodiscard]] std::uintptr_t font_id() const noexcept;
    [[nodiscard]] std::string family() const;

    std::uint32_t id;
    float x;
    float y;

    // UTF-8 byte offset, independent of the number and order of output glyphs.
    std::uint32_t cluster = 0;
    std::shared_ptr<const struct glyph_bitmap> image = nullptr;
    float advance = 0;
    float origin = 0;
    rect ink{};

private:
    friend struct details::glyph_access;

    std::shared_ptr<details::font_face> m_face;
};

struct text_line
{
    std::size_t begin;
    std::size_t end;
    float width;

    std::size_t text_begin = 0;
    std::size_t text_end = 0;

    float y = 0;
    float height = 0;
    float baseline = 0;
    float offset = 0;

    bool rtl = false;
};

struct text_caret
{
    std::size_t offset;
    vec2 position;
    text_affinity affinity = text_affinity::downstream;
    std::size_t line = 0;
};

struct text_selection_cell
{
    std::size_t begin;
    std::size_t end;

    rect bounds;
};

/// Immutable text geometry. Copies share storage and retain their exact fonts.
/// A default-constructed or moved-from layout is an empty value.
class layout
{
public:
    layout() = default;

    [[nodiscard]] rect bounds() const noexcept;
    [[nodiscard]] rect ink_bounds() const noexcept;

    [[nodiscard]] float baseline() const noexcept;
    [[nodiscard]] float font_size() const noexcept;
    [[nodiscard]] float line_height() const noexcept;
    [[nodiscard]] float raster_scale() const noexcept;

    [[nodiscard]] std::span<const glyph> glyphs() const noexcept;
    [[nodiscard]] std::span<const text_line> lines() const noexcept;
    [[nodiscard]] std::span<const text_caret> carets() const noexcept;
    [[nodiscard]] std::span<const text_selection_cell> selection_cells() const noexcept;

    [[nodiscard]] rect caret_bounds(text_position position) const;
    [[nodiscard]] text_position hit_test(vec2 point) const;
    [[nodiscard]] text_position move_visual(text_position position, bool forward) const;
    [[nodiscard]] std::vector<rect> selection_bounds(std::size_t begin, std::size_t end) const;

    /// Creates an independently owned aligned copy outside engine cache budgets.
    [[nodiscard]] layout aligned(float width, text_alignment alignment) const;

private:
    friend struct details::layout_access;

    explicit layout(std::shared_ptr<const details::layout_data> data)
        : m_data(std::move(data))
    {
    }

    std::shared_ptr<const details::layout_data> m_data;
};

} // namespace centypo

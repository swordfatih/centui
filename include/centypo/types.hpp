#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace centypo
{

struct rect
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    constexpr bool operator==(const rect&) const = default;
};

struct vec2
{
    float x = 0;
    float y = 0;

    constexpr bool operator==(const vec2&) const = default;
};

enum class text_alignment
{
    left,
    center,
    right,
    start,
    end
};

enum class text_affinity
{
    downstream,
    upstream
};

/// \brief A UTF-8 byte offset and the visual side of a bidi or wrapped boundary.
struct text_position
{
    std::size_t offset = 0;
    text_affinity affinity = text_affinity::downstream;

    bool operator==(const text_position&) const = default;
};

enum class pixel_format
{
    alpha,
    rgba
};

/// \brief CPU image pixels stored in row-major order.
struct bitmap
{
    int width = 0;
    int height = 0;

    std::vector<std::uint8_t> pixels;
    pixel_format format = pixel_format::rgba;
};

/// \brief A rasterized glyph with its baseline bearing.
struct glyph_bitmap : bitmap
{
    glyph_bitmap(int width, int height, int left, int top, std::vector<std::uint8_t> pixels);

    int left = 0;
    int top = 0;
};

} // namespace centypo

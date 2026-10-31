#include "glyph_image.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>

namespace centypo::details
{
namespace
{

std::uint8_t straight_channel(std::uint8_t value, std::uint8_t alpha)
{
    if(alpha == 0)
    {
        return 0;
    }

    const auto straight = (value * 255u + alpha / 2u) / alpha;

    return static_cast<std::uint8_t>(std::min(255u, straight));
}

void copy_bgra_row(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination)
{
    for(std::size_t pixel = 0; pixel < destination.size(); pixel += 4)
    {
        const auto alpha = source[pixel + 3];

        destination[pixel] = straight_channel(source[pixel + 2], alpha);
        destination[pixel + 1] = straight_channel(source[pixel + 1], alpha);
        destination[pixel + 2] = straight_channel(source[pixel], alpha);
        destination[pixel + 3] = alpha;
    }
}

void copy_mono_row(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination)
{
    for(std::size_t pixel = 0; pixel < destination.size(); ++pixel)
    {
        const auto mask = 0x80 >> (pixel % 8);

        destination[pixel] = (source[pixel / 8] & mask) ? 255 : 0;
    }
}

void copy_gray_row(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination, unsigned gray_levels)
{
    if(gray_levels <= 1)
    {
        std::fill(destination.begin(), destination.end(), 0);
        return;
    }

    for(std::size_t pixel = 0; pixel < destination.size(); ++pixel)
    {
        destination[pixel] = static_cast<std::uint8_t>(source[pixel] * 255u / (gray_levels - 1));
    }
}

} // namespace

glyph_bitmap copy_glyph_bitmap(const FT_Bitmap& bitmap, int left, int top)
{
    const bool empty = bitmap.width == 0 || bitmap.rows == 0;
    const bool supported = bitmap.pixel_mode == FT_PIXEL_MODE_GRAY || bitmap.pixel_mode == FT_PIXEL_MODE_BGRA ||
                           bitmap.pixel_mode == FT_PIXEL_MODE_MONO;

    if(!supported && !empty)
    {
        throw std::runtime_error("Unsupported glyph bitmap format");
    }

    const auto channels = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA ? std::size_t{4} : std::size_t{1};
    const auto max_dimension = static_cast<unsigned int>(std::numeric_limits<int>::max());
    const auto max_bytes = std::numeric_limits<std::size_t>::max();

    if(bitmap.width > max_dimension || bitmap.rows > max_dimension ||
       (bitmap.width != 0 && bitmap.rows > max_bytes / bitmap.width / channels))
    {
        throw std::length_error("Glyph bitmap dimensions are not representable");
    }

    glyph_bitmap result{
        static_cast<int>(bitmap.width),
        static_cast<int>(bitmap.rows),
        left,
        top,
        {},
    };

    result.format = channels == 4 ? pixel_format::rgba : pixel_format::alpha;
    result.pixels.resize(static_cast<std::size_t>(result.width) * result.height * channels);

    if(empty)
    {
        return result;
    }

    const auto output_row_bytes = static_cast<std::size_t>(result.width) * channels;
    const auto source_row_bytes =
        bitmap.pixel_mode == FT_PIXEL_MODE_MONO ? (static_cast<std::size_t>(result.width) + 7) / 8 : output_row_bytes;
    const auto pitch = static_cast<std::int64_t>(bitmap.pitch);
    const auto stride = static_cast<std::uint64_t>(pitch < 0 ? -pitch : pitch);

    if(!bitmap.buffer || stride < source_row_bytes)
    {
        throw std::invalid_argument("Invalid glyph bitmap buffer or pitch");
    }

    const auto max_offset = static_cast<std::uint64_t>(std::numeric_limits<std::ptrdiff_t>::max());

    if(result.height > 1 && stride > max_offset / (result.height - 1))
    {
        throw std::length_error("Glyph row offsets are not representable");
    }

    for(int row = 0; row < result.height; ++row)
    {
        const auto offset = row * static_cast<std::ptrdiff_t>(bitmap.pitch);
        const std::span<const std::uint8_t> source{bitmap.buffer + offset, source_row_bytes};
        const auto destination = std::span{result.pixels}.subspan(row * output_row_bytes, output_row_bytes);

        switch(bitmap.pixel_mode)
        {
        case FT_PIXEL_MODE_BGRA:
            // FreeType stores premultiplied BGRA; the renderer consumes straight RGBA.
            copy_bgra_row(source, destination);
            break;

        case FT_PIXEL_MODE_MONO:
            copy_mono_row(source, destination);
            break;

        case FT_PIXEL_MODE_GRAY:
            copy_gray_row(source, destination, bitmap.num_grays);
            break;
        }
    }

    return result;
}

} // namespace centypo::details

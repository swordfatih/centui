#include <centui/render.hpp>

namespace centui
{

[[nodiscard]] rect caret_bounds(const text_layout& layout, std::size_t offset)
{
    rect result{0, 0, 1, layout.line_height};
    std::size_t distance = std::numeric_limits<std::size_t>::max();

    for(const auto& caret: layout.carets)
    {
        const auto delta = caret.offset > offset ? caret.offset - offset : offset - caret.offset;

        if(delta <= distance)
        {
            distance = delta;
            result.x = caret.position.x;
            result.y = caret.position.y;
        }
    }

    return result;
}

[[nodiscard]] std::size_t text_hit(const text_layout& layout, vec2 point)
{
    std::size_t result = 0;
    float best_y = std::numeric_limits<float>::infinity();
    float best_x = std::numeric_limits<float>::infinity();

    for(const auto& caret: layout.carets)
    {
        const auto dy = std::abs(point.y - caret.position.y - layout.line_height / 2);
        const auto dx = std::abs(point.x - caret.position.x);

        if(dy < best_y || (dy == best_y && dx < best_x))
        {
            best_y = dy;
            best_x = dx;
            result = caret.offset;
        }
    }

    return result;
}

[[nodiscard]] std::vector<rect> selection_bounds(const text_layout& layout, std::size_t begin, std::size_t end)
{
    if(begin > end)
    {
        std::swap(begin, end);
    }

    std::vector<rect> result;

    for(std::size_t index = 0; index < layout.lines.size() && begin != end; ++index)
    {
        const auto& line = layout.lines[index];

        if(end <= line.text_begin || begin > line.text_end)
        {
            continue;
        }

        const auto start = std::clamp(begin, line.text_begin, line.text_end);
        const auto finish = std::clamp(end, line.text_begin, line.text_end);
        const auto y = index * layout.line_height;
        float left = 0;
        float right = line.width;

        for(const auto& caret: layout.carets)
        {
            if(std::abs(caret.position.y - y) < layout.line_height / 2 && caret.offset == start)
            {
                left = caret.position.x;
            }

            if(std::abs(caret.position.y - y) < layout.line_height / 2 && caret.offset == finish)
            {
                right = caret.position.x;
            }
        }

        result.push_back({std::min(left, right), y, std::max(1.0f, std::abs(right - left)), layout.line_height});
    }

    return result;
}

glyph_bitmap::glyph_bitmap(int width, int height, int left, int top, std::vector<std::uint8_t> pixels)
    : bitmap{width, height, std::move(pixels), pixel_format::alpha},
      left(left),
      top(top)
{
}

void validate_bitmap(const bitmap& image)
{
    if(image.format != pixel_format::alpha && image.format != pixel_format::rgba)
    {
        throw std::invalid_argument("Unknown bitmap pixel format");
    }

    const auto channels = image.format == pixel_format::rgba ? std::size_t{4} : std::size_t{1};

    if(image.width <= 0 || image.height <= 0 ||
       static_cast<std::size_t>(image.width) >
           std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(image.height) / channels ||
       image.pixels.size() != static_cast<std::size_t>(image.width) * image.height * channels)
    {
        throw std::invalid_argument("Image pixels must match positive bitmap dimensions");
    }
}

[[nodiscard]] std::vector<std::uint8_t> rgba_pixels(const bitmap& image)
{
    validate_bitmap(image);

    if(image.format == pixel_format::rgba)
    {
        return image.pixels;
    }

    if(image.pixels.size() > std::numeric_limits<std::size_t>::max() / 4)
    {
        throw std::length_error("Image exceeds RGBA storage limits");
    }

    std::vector<std::uint8_t> result(image.pixels.size() * 4, 255);

    for(std::size_t index = 0; index < image.pixels.size(); ++index)
    {
        result[index * 4 + 3] = image.pixels[index];
    }

    return result;
}

[[nodiscard]] rect intersection(rect a, rect b) noexcept
{
    const auto x = std::max(a.x, b.x);
    const auto y = std::max(a.y, b.y);

    return {
        x,
        y,
        std::max(0.0f, std::min(a.x + a.width, b.x + b.width) - x),
        std::max(0.0f, std::min(a.y + a.height, b.y + b.height) - y)
    };
}

} // namespace centui

#include <centui/render.hpp>

namespace centui
{

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

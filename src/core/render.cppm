module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

export module centui.render;

export import centui.types;

export namespace centui
{

struct shaped_glyph
{
    std::uint32_t id;
    float         x;
    float         y;
    // UTF-8 byte offset, independent of the number and order of output glyphs.
    std::uint32_t cluster = 0;
    std::shared_ptr<const struct glyph_bitmap> image = nullptr;
    float advance = 0;
    float origin = 0;
};

struct text_layout
{
    struct line
    {
        std::size_t begin;
        std::size_t end;
        float width;
        std::size_t text_begin = 0;
        std::size_t text_end = 0;
    };

    std::vector<shaped_glyph> glyphs;
    rect                     bounds{};
    float                    ascender = 0;
    std::vector<line>         lines;
    struct caret
    {
        std::size_t offset;
        vec2 position;
    };

    float line_height = 0;
    float raster_scale = 1;
    std::vector<caret> carets;
};

[[nodiscard]] inline rect caret_bounds(const text_layout& layout, std::size_t offset)
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

[[nodiscard]] inline std::size_t text_hit(const text_layout& layout, vec2 point)
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

[[nodiscard]] inline std::vector<rect> selection_bounds(const text_layout& layout, std::size_t begin, std::size_t end)
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

enum class pixel_format
{
    alpha,
    rgba
};

struct bitmap
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
    pixel_format format = pixel_format::rgba;
};

struct glyph_bitmap : bitmap
{
    glyph_bitmap(int width, int height, int left, int top, std::vector<std::uint8_t> pixels)
        : bitmap{width, height, std::move(pixels), pixel_format::alpha}, left(left), top(top)
    {
    }

    int left = 0;
    int top = 0;
};

inline void validate_bitmap(const bitmap& image)
{
    if(image.format != pixel_format::alpha && image.format != pixel_format::rgba)
    {
        throw std::invalid_argument("Unknown bitmap pixel format");
    }

    const auto channels = image.format == pixel_format::rgba ? std::size_t{4} : std::size_t{1};
    if(image.width <= 0 || image.height <= 0 ||
       static_cast<std::size_t>(image.width) > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(image.height) / channels ||
       image.pixels.size() != static_cast<std::size_t>(image.width) * image.height * channels)
    {
        throw std::invalid_argument("Image pixels must match positive bitmap dimensions");
    }
}

[[nodiscard]] inline std::vector<std::uint8_t> rgba_pixels(const bitmap& image)
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

struct rectangle_command
{
    rect bounds;
    centui::color color;
    rect clip;
};

struct text_command
{
    rect bounds;
    centui::color color;
    std::shared_ptr<const text_layout> shaped;
    text_alignment alignment = text_alignment::left;
    rect clip{};
};

struct image_command
{
    rect bounds;
    rect clip;
    std::shared_ptr<const bitmap> image;
    centui::color color = {255, 255, 255, 255};
};

using draw_command = std::variant<rectangle_command, text_command, image_command>;

// Document-owned storage, borrowed synchronously until its next preparation.
// Commands retain immutable CPU resources independently of the font cache.
struct draw_list
{
    centui::viewport viewport;
    std::vector<draw_command> commands;
};

class renderer
{
public:
    virtual ~renderer() = default;
    [[nodiscard]] virtual centui::viewport viewport() const = 0;
    virtual void draw(const draw_list&) = 0;
};

[[nodiscard]] inline rect intersection(rect a, rect b) noexcept
{
    const auto x = std::max(a.x, b.x);
    const auto y = std::max(a.y, b.y);
    return {x, y, std::max(0.0f, std::min(a.x + a.width, b.x + b.width) - x),
                  std::max(0.0f, std::min(a.y + a.height, b.y + b.height) - y)};
}

struct glyph_quad
{
    rect bounds;
    rect uv;
    std::shared_ptr<const glyph_bitmap> image;
    centui::color color;
};

namespace details
{

template <typename Draw>
void draw_image(const image_command& command, rect viewport, Draw&& draw)
{
    if(!command.image || command.color.a == 0 || command.bounds.width <= 0 || command.bounds.height <= 0)
    {
        return;
    }

    validate_bitmap(*command.image);
    const auto bounds = intersection(intersection(command.bounds, command.clip), viewport);
    if(bounds.width > 0 && bounds.height > 0)
    {
        const rect uv{(bounds.x - command.bounds.x) / command.bounds.width,
                      (bounds.y - command.bounds.y) / command.bounds.height,
                      bounds.width / command.bounds.width, bounds.height / command.bounds.height};
        draw(bounds, uv);
    }
}

// Shared placement and clipping. Coordinates point down; HB offsets point up.
// UV coordinates are normalized; command colors use straight RGBA.
template <typename draw_function>
void for_each_glyph(const text_command& command, rect viewport, draw_function&& draw)
{
    if(!command.shaped || command.color.a == 0)
    {
        return;
    }
    const auto clip = intersection(intersection(command.bounds, command.clip), viewport);
    if(clip.width <= 0 || clip.height <= 0)
    {
        return;
    }
    const auto& shaped = *command.shaped;
    for(const auto& line: shaped.lines)
    {
        if(line.begin > line.end || line.end > shaped.glyphs.size())
        {
            throw std::invalid_argument("Glyph line is outside its prepared run");
        }
        const auto spare = std::max(0.0f, command.bounds.width - line.width);
        const auto offset = command.alignment == text_alignment::center ? spare / 2 :
                            command.alignment == text_alignment::right ? spare : 0;
        for(auto i = line.begin; i < line.end; ++i)
        {
            const auto& glyph = shaped.glyphs[i];
            if(!glyph.image || glyph.image->width <= 0 || glyph.image->height <= 0)
            {
                continue;
            }
            const auto& image = *glyph.image;
            const auto width = static_cast<std::size_t>(image.width);
            const auto height = static_cast<std::size_t>(image.height);
            if(width > std::numeric_limits<std::size_t>::max() / height || image.pixels.size() != width * height)
            {
                throw std::invalid_argument("Glyph pixels do not match their image dimensions");
            }
            const auto scale = shaped.raster_scale;
            const rect original{std::round((command.bounds.x + offset + glyph.x) * scale + image.left) / scale,
                                std::round((command.bounds.y + shaped.ascender - glyph.y) * scale - image.top) / scale,
                                image.width / scale, image.height / scale};
            const auto bounds = intersection(original, clip);
            if(bounds.width <= 0 || bounds.height <= 0)
            {
                continue;
            }
            draw(glyph_quad{bounds, {(bounds.x - original.x) / original.width,
                                     (bounds.y - original.y) / original.height,
                                     bounds.width / original.width, bounds.height / original.height},
                            glyph.image, command.color});
        }
    }
}

} // namespace details
} // namespace centui

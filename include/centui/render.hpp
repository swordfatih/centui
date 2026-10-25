#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <centui/types.hpp>

namespace centui
{

/// \brief A positioned glyph and its original UTF-8 byte offset.
struct shaped_glyph
{
    std::uint32_t id;
    float x;
    float y;
    // UTF-8 byte offset, independent of the number and order of output glyphs.
    std::uint32_t cluster = 0;
    std::shared_ptr<const struct glyph_bitmap> image = nullptr;
    float advance = 0;
    float origin = 0;
};

/// \brief Prepared glyphs, line metrics, and caret positions in logical units.
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
    rect bounds{};
    float ascender = 0;
    std::vector<line> lines;

    struct caret
    {
        std::size_t offset;
        vec2 position;
    };

    float line_height = 0;
    float raster_scale = 1;
    std::vector<caret> carets;
};

[[nodiscard]] rect caret_bounds(const text_layout& layout, std::size_t offset);

[[nodiscard]] std::size_t text_hit(const text_layout& layout, vec2 point);

[[nodiscard]] std::vector<rect> selection_bounds(const text_layout& layout, std::size_t begin, std::size_t end);

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

void validate_bitmap(const bitmap& image);

[[nodiscard]] std::vector<std::uint8_t> rgba_pixels(const bitmap& image);

struct text_command
{
    rect bounds;
    centui::color color;
    std::shared_ptr<const text_layout> shaped;
    text_alignment alignment = text_alignment::left;
    rect clip{};
};

enum class image_sampling
{
    linear,
    nearest
};

struct image_command
{
    rect bounds;
    rect clip;
    std::shared_ptr<const bitmap> image;
    centui::color color = {255, 255, 255, 255};
    image_sampling sampling = image_sampling::linear;
};

/// \brief A clipped quad in logical units. An absent image means a solid color.
struct draw_quad
{
    rect bounds;
    rect uv{0, 0, 1, 1};
    std::shared_ptr<const bitmap> image;
    centui::color color{255, 255, 255, 255};
    image_sampling sampling = image_sampling::linear;
};

// Document-owned storage, borrowed synchronously until its next preparation.
// Commands retain immutable CPU resources independently of the font cache.
/// \brief Prepared commands that retain their image and text resources.
struct draw_list
{
    centui::viewport viewport;
    std::vector<draw_quad> commands;
};

/// \brief Backend interface for frame presentation and drawing a prepared list.
class renderer
{
public:
    virtual ~renderer() = default;

    [[nodiscard]] virtual centui::viewport viewport() const = 0;

    /// \brief Begins a frame using the backend's default clear color; false skips rendering.
    [[nodiscard]] virtual bool begin_frame() = 0;

    virtual void draw(const draw_list&) = 0;

    virtual void end_frame() = 0;
};

[[nodiscard]] rect intersection(rect a, rect b) noexcept;

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
        const rect uv{
            (bounds.x - command.bounds.x) / command.bounds.width,
            (bounds.y - command.bounds.y) / command.bounds.height,
            bounds.width / command.bounds.width,
            bounds.height / command.bounds.height
        };

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
        const auto offset = command.alignment == text_alignment::center  ? spare / 2
                            : command.alignment == text_alignment::right ? spare
                                                                         : 0;

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
            const rect original{
                std::round((command.bounds.x + offset + glyph.x) * scale + image.left) / scale,
                std::round((command.bounds.y + shaped.ascender - glyph.y) * scale - image.top) / scale,
                image.width / scale,
                image.height / scale
            };

            const auto bounds = intersection(original, clip);

            if(bounds.width <= 0 || bounds.height <= 0)
            {
                continue;
            }

            draw(
                glyph_quad{
                    bounds,
                    {(bounds.x - original.x) / original.width,
                     (bounds.y - original.y) / original.height,
                     bounds.width / original.width,
                     bounds.height / original.height},
                    glyph.image,
                    command.color
                }
            );
        }
    }
}

} // namespace details
} // namespace centui

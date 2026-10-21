module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
};

struct text_layout
{
    struct line
    {
        std::size_t begin;
        std::size_t end;
        float width;
    };

    std::vector<shaped_glyph> glyphs;
    rect                     bounds{};
    float                    ascender = 0;
    std::vector<line>         lines;
};

struct glyph_bitmap
{
    int                       width;
    int                       height;
    int                       left;
    int                       top;
    std::vector<std::uint8_t> pixels;
};

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

using draw_command = std::variant<rectangle_command, text_command>;

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
    [[nodiscard]] virtual centui::viewport viewport() const noexcept = 0;
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
            const rect original{command.bounds.x + offset + glyph.x + image.left,
                                command.bounds.y + shaped.ascender - glyph.y - image.top,
                                static_cast<float>(image.width), static_cast<float>(image.height)};
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

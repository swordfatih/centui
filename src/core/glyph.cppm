
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

export module centui.glyph;

import centui.unit;

export namespace centui
{

struct shaped_glyph
{
    std::uint32_t id;
    float         x;
    float         y;
};

struct text_layout
{
    std::vector<shaped_glyph> glyphs;
    rect                     bounds{};
    float                    ascender = 0;
};

struct glyph_bitmap
{
    int                       width;
    int                       height;
    int                       left;
    int                       top;
    std::vector<std::uint8_t> pixels;
};


class font
{
public:
    font();
    explicit font(const std::filesystem::path& path);
    ~font();

    font(const font&) = delete;
    font& operator=(const font&) = delete;

    font(font&&) noexcept;
    font& operator=(font&&) noexcept;

    [[nodiscard]] text_layout layout_text(std::string_view text, float size);
    [[nodiscard]] rect        measure(std::string_view text, float size);

    [[nodiscard]] std::vector<shaped_glyph> shape(std::string_view text, float size);
    [[nodiscard]] glyph_bitmap              rasterize(std::uint32_t id, float size);
    [[nodiscard]] float                     ascender(float size);

private:
    struct data;
    std::unique_ptr<data> _data;
};

} // namespace centui

namespace centui
{

struct font::data
{
    struct library_deleter
    {
        void operator()(FT_Library library) const noexcept
        {
            FT_Done_FreeType(library);
        }
    };

    struct face_deleter
    {
        void operator()(FT_Face face) const noexcept
        {
            FT_Done_Face(face);
        }
    };

    struct font_deleter
    {
        void operator()(hb_font_t* font) const noexcept
        {
            hb_font_destroy(font);
        }
    };

    // Reverse destruction: HarfBuzz, face, then FreeType library.
    std::unique_ptr<std::remove_pointer_t<FT_Library>, library_deleter> library;
    std::unique_ptr<std::remove_pointer_t<FT_Face>, face_deleter>       face;
    std::unique_ptr<hb_font_t, font_deleter>                           font;

    void set_size(float size)
    {
        if(size <= 0 || !std::isfinite(size) ||
           static_cast<double>(std::round(size)) > std::numeric_limits<FT_UInt>::max())
        {
            throw std::invalid_argument("Font size must be positive, finite, and representable");
        }

        if(FT_Set_Pixel_Sizes(face.get(), 0, static_cast<FT_UInt>(std::max(1.0f, std::round(size)))))
        {
            throw std::runtime_error("Failed to set font size");
        }
    }
};

namespace details
{

std::filesystem::path default_font_path()
{
#ifdef _WIN32
    const auto* windows = std::getenv("WINDIR");

    return (windows ? std::filesystem::path{windows} : std::filesystem::path{"C:\\Windows"}) / "Fonts" / "segoeui.ttf";
#else
    throw std::runtime_error("Default system font discovery is not implemented on this platform");
#endif
}

} // namespace details

font::font() : font(details::default_font_path())
{
}

font::font(const std::filesystem::path& path) : _data(std::make_unique<data>())
{
    if(FT_Init_FreeType(std::out_ptr(_data->library)))
    {
        throw std::runtime_error("Failed to initialize FreeType");
    }

    if(FT_New_Face(_data->library.get(), path.string().c_str(), 0, std::out_ptr(_data->face)))
    {
        throw std::runtime_error("Failed to load font: " + path.string());
    }

    if(FT_Set_Pixel_Sizes(_data->face.get(), 0, 16))
    {
        throw std::runtime_error("Failed to set font size");
    }

    _data->font.reset(hb_ft_font_create_referenced(_data->face.get()));

    if(!_data->font)
    {
        throw std::runtime_error("Failed to initialize HarfBuzz font");
    }
}

font::~font() = default;

font::font(font&&) noexcept = default;
font& font::operator=(font&&) noexcept = default;

rect font::measure(std::string_view text, float size)
{
    return layout_text(text, size).bounds;
}

std::vector<shaped_glyph> font::shape(std::string_view text, float size)
{
    return layout_text(text, size).glyphs;
}

text_layout font::layout_text(std::string_view text, float size)
{
    _data->set_size(size);

    if(text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::length_error("Text exceeds HarfBuzz input limits");
    }

    hb_ft_font_changed(_data->font.get());

    std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer{hb_buffer_create(), hb_buffer_destroy};

    if(!buffer)
    {
        throw std::runtime_error("Failed to create HarfBuzz buffer");
    }

    hb_buffer_add_utf8(buffer.get(), text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_buffer_guess_segment_properties(buffer.get());
    hb_shape(_data->font.get(), buffer.get(), nullptr, 0);

    unsigned int count = 0;
    const auto*  infos = hb_buffer_get_glyph_infos(buffer.get(), &count);
    const auto*  positions = hb_buffer_get_glyph_positions(buffer.get(), nullptr);

    text_layout result;
    result.glyphs.reserve(count);

    float x = 0.0f;
    float y = 0.0f;

    for(unsigned int i = 0; i < count; ++i)
    {
        result.glyphs.push_back({infos[i].codepoint,
                                 x + positions[i].x_offset / 64.0f,
                                 y + positions[i].y_offset / 64.0f});

        x += positions[i].x_advance / 64.0f;
        y += positions[i].y_advance / 64.0f;
    }

    result.bounds.width = x;
    result.bounds.height = _data->face->size->metrics.height / 64.0f;
    result.ascender = _data->face->size->metrics.ascender / 64.0f;
    return result;
}

glyph_bitmap font::rasterize(std::uint32_t id, float size)
{
    _data->set_size(size);

    if(FT_Load_Glyph(_data->face.get(), id, FT_LOAD_DEFAULT))
    {
        throw std::runtime_error("Failed to load glyph");
    }

    if(FT_Render_Glyph(_data->face->glyph, FT_RENDER_MODE_NORMAL))
    {
        throw std::runtime_error("Failed to rasterize glyph");
    }

    const auto& slot = *_data->face->glyph;
    const auto& bitmap = slot.bitmap;

    if(bitmap.pixel_mode != FT_PIXEL_MODE_GRAY && bitmap.width > 0 && bitmap.rows > 0)
    {
        throw std::runtime_error("Unsupported glyph bitmap format");
    }

    glyph_bitmap result{
        static_cast<int>(bitmap.width),
        static_cast<int>(bitmap.rows),
        slot.bitmap_left,
        slot.bitmap_top,
        {}};

    result.pixels.resize(static_cast<std::size_t>(result.width) * result.height);

    for(int row = 0; row < result.height; ++row)
    {
        const auto* source = bitmap.buffer + (bitmap.pitch >= 0 ? row * bitmap.pitch : (result.height - 1 - row) * -bitmap.pitch);
        std::copy_n(source, result.width, result.pixels.data() + row * result.width);
    }

    return result;
}

float font::ascender(float size)
{
    _data->set_size(size);

    return _data->face->size->metrics.ascender / 64.0f;
}

} // namespace centui

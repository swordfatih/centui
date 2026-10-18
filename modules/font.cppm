
module;

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

export module centui.font;

import std;
import centui.unit;

export namespace centui
{

struct shaped_glyph
{
    std::uint32_t id;
    float         x;
    float         y;
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

    [[nodiscard]] rect measure(std::string_view text, float size);

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
    FT_Library library = nullptr;
    FT_Face    face = nullptr;
    hb_font_t* font = nullptr;

    ~data()
    {
        if(font)
        {
            hb_font_destroy(font);
        }

        if(face)
        {
            FT_Done_Face(face);
        }

        if(library)
        {
            FT_Done_FreeType(library);
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
    if(FT_Init_FreeType(&_data->library))
    {
        throw std::runtime_error("Failed to initialize FreeType");
    }

    if(FT_New_Face(_data->library, path.string().c_str(), 0, &_data->face))
    {
        throw std::runtime_error("Failed to load font: " + path.string());
    }

    if(FT_Set_Pixel_Sizes(_data->face, 0, 16))
    {
        throw std::runtime_error("Failed to set font size");
    }

    _data->font = hb_ft_font_create_referenced(_data->face);

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
    if(size <= 0 || !std::isfinite(size))
    {
        throw std::invalid_argument("Font size must be positive and finite");
    }

    if(FT_Set_Pixel_Sizes(_data->face, 0, static_cast<FT_UInt>(std::max(1.0f, std::round(size)))))
    {
        throw std::runtime_error("Failed to set font size");
    }

    hb_ft_font_changed(_data->font);

    auto* buffer = hb_buffer_create();

    if(!buffer)
    {
        throw std::runtime_error("Failed to create HarfBuzz buffer");
    }

    hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(_data->font, buffer, nullptr, 0);

    unsigned int count = 0;
    const auto*  positions = hb_buffer_get_glyph_positions(buffer, &count);

    float width = 0;

    for(unsigned int i = 0; i < count; ++i)
    {
        width += positions[i].x_advance / 64.0f;
    }

    hb_buffer_destroy(buffer);

    return {
        .width = width,
        .height = _data->face->size->metrics.height / 64.0f};
}


std::vector<shaped_glyph> font::shape(std::string_view text, float size)
{
    if(size <= 0 || !std::isfinite(size))
    {
        throw std::invalid_argument("Invalid font size");
    }

    if(FT_Set_Pixel_Sizes(_data->face, 0, static_cast<FT_UInt>(std::max(1.0f, std::round(size)))))
    {
        throw std::runtime_error("Failed to set font size");
    }

    hb_ft_font_changed(_data->font);

    auto* buffer = hb_buffer_create();

    if(!buffer)
    {
        throw std::runtime_error("Failed to create HarfBuzz buffer");
    }

    hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(_data->font, buffer, nullptr, 0);

    unsigned int count = 0;
    const auto*  infos = hb_buffer_get_glyph_infos(buffer, &count);
    const auto*  positions = hb_buffer_get_glyph_positions(buffer, nullptr);

    std::vector<shaped_glyph> result;
    result.reserve(count);

    float x = 0.0f;
    float y = 0.0f;

    for(unsigned int i = 0; i < count; ++i)
    {
        result.push_back({infos[i].codepoint,
                          x + positions[i].x_offset / 64.0f,
                          y + positions[i].y_offset / 64.0f});

        x += positions[i].x_advance / 64.0f;
        y += positions[i].y_advance / 64.0f;
    }

    hb_buffer_destroy(buffer);
    return result;
}

glyph_bitmap font::rasterize(std::uint32_t id, float size)
{
    if(size <= 0 || !std::isfinite(size))
    {
        throw std::invalid_argument("Invalid font size");
    }

    if(FT_Set_Pixel_Sizes(_data->face, 0, static_cast<FT_UInt>(std::max(1.0f, std::round(size)))))
    {
        throw std::runtime_error("Failed to set font size");
    }

    if(FT_Load_Glyph(_data->face, id, FT_LOAD_DEFAULT))
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
    if(size <= 0 || !std::isfinite(size))
    {
        throw std::invalid_argument("Invalid font size");
    }

    if(FT_Set_Pixel_Sizes(_data->face, 0, static_cast<FT_UInt>(std::max(1.0f, std::round(size)))))
    {
        throw std::runtime_error("Failed to set font size");
    }

    return _data->face->size->metrics.ascender / 64.0f;
}

} // namespace centui

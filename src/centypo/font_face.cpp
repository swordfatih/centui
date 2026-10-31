#include "font_face.hpp"

#include "glyph_image.hpp"

#include <fstream>
#include <map>
#include <span>

#include <centypo/engine.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_MULTIPLE_MASTERS_H
#include <hb-ft.h>
#include <hb-ot.h>
#include <hb.h>

namespace centypo::details
{

namespace
{

/// \brief Adopts a native output handle when the API call completes.
template <typename T, typename Deleter>
class output_handle
{
public:
    using pointer = typename std::unique_ptr<T, Deleter>::pointer;

    explicit output_handle(std::unique_ptr<T, Deleter>& owner) noexcept
        : m_owner(owner)
    {
    }

    ~output_handle()
    {
        m_owner.reset(m_value);
    }

    output_handle(const output_handle&) = delete;
    output_handle& operator=(const output_handle&) = delete;

    operator pointer*() noexcept
    {
        return &m_value;
    }

private:
    std::unique_ptr<T, Deleter>& m_owner;
    pointer m_value = nullptr;
};

} // namespace

struct font_face::data
{
    std::shared_ptr<const void> identity = std::make_shared<const int>(0);

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

    // Snapshot the input. FT_Face borrows these bytes until its destruction.
    std::shared_ptr<const std::vector<unsigned char>> bytes;

    // Reverse destruction: HarfBuzz, face, then FreeType library and bytes.
    std::unique_ptr<std::remove_pointer_t<FT_Library>, library_deleter> library;
    std::unique_ptr<std::remove_pointer_t<FT_Face>, face_deleter> face;
    std::unique_ptr<hb_font_t, font_deleter> font;

    float size_scale = 1;

    void set_size(float size)
    {
        if(size <= 0 || !std::isfinite(size) ||
           static_cast<double>(size) * 64 > std::numeric_limits<std::int32_t>::max())
        {
            throw std::invalid_argument("Font size must be positive, finite, and representable");
        }

        size_scale = 1;
        if(FT_Set_Char_Size(
               face.get(),
               0,
               static_cast<FT_F26Dot6>(std::max(1.0, std::round(static_cast<double>(size) * 64))),
               72,
               72
           ))
        {
            if(face->num_fixed_sizes == 0)
            {
                throw std::runtime_error("Failed to set font size");
            }

            int best = 0;

            for(int i = 1; i < face->num_fixed_sizes; ++i)
            {
                if(std::abs(face->available_sizes[i].y_ppem / 64.0f - size) <
                   std::abs(face->available_sizes[best].y_ppem / 64.0f - size))
                {
                    best = i;
                }
            }

            if(FT_Select_Size(face.get(), best) || face->size->metrics.y_ppem == 0)
            {
                throw std::runtime_error("Failed to select font bitmap strike");
            }

            size_scale = size / face->size->metrics.y_ppem;
        }
        else if(size < 1)
        {
            size_scale = size;
        }
    }
};

font_face::font_face(
    std::shared_ptr<const std::vector<unsigned char>> bytes,
    long index,
    std::optional<std::pair<int, font_slant>> configuration
)
    : m_data(std::make_unique<data>())
{
    if(!bytes || bytes->empty() || bytes->size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max()))
    {
        throw std::invalid_argument("Font bytes must be nonempty and representable");
    }

    m_data->bytes = std::move(bytes);
    if(FT_Init_FreeType(output_handle{m_data->library}))
    {
        throw std::runtime_error("Failed to initialize FreeType");
    }

    if(FT_New_Memory_Face(
           m_data->library.get(),
           m_data->bytes->data(),
           static_cast<FT_Long>(m_data->bytes->size()),
           index,
           output_handle{m_data->face}
       ))
    {
        throw std::runtime_error("Failed to load font bytes");
    }

    if(configuration && FT_HAS_MULTIPLE_MASTERS(m_data->face.get()))
    {
        FT_MM_Var* raw = nullptr;
        if(FT_Get_MM_Var(m_data->face.get(), &raw))
        {
            throw std::runtime_error("Cannot inspect variable font");
        }

        const auto destroy = [library = m_data->library.get()](FT_MM_Var* value)
        {
            FT_Done_MM_Var(library, value);
        };
        std::unique_ptr<FT_MM_Var, decltype(destroy)> axes{raw, destroy};
        std::vector<FT_Fixed> coordinates(axes->num_axis);
        for(FT_UInt i = 0; i < axes->num_axis; ++i)
        {
            const auto& axis = axes->axis[i];
            auto value = axis.def;

            if(axis.tag == FT_MAKE_TAG('w', 'g', 'h', 't'))
            {
                value = configuration->first * 65536L;
            }
            if(axis.tag == FT_MAKE_TAG('i', 't', 'a', 'l'))
            {
                value = configuration->second == font_slant::italic ? 65536L : 0;
            }
            if(axis.tag == FT_MAKE_TAG('s', 'l', 'n', 't'))
            {
                value = configuration->second == font_slant::oblique ? -12 * 65536L : 0;
            }

            coordinates[i] = std::clamp(value, axis.minimum, axis.maximum);
        }

        if(FT_Set_Var_Design_Coordinates(m_data->face.get(), axes->num_axis, coordinates.data()))
        {
            throw std::runtime_error("Cannot configure variable font");
        }
    }

    m_data->set_size(16);
    if(m_data->face->size->metrics.height <= 0)
    {
        throw std::runtime_error("Font has invalid line metrics");
    }

    m_data->font.reset(hb_ft_font_create_referenced(m_data->face.get()));

    if(!m_data->font)
    {
        throw std::runtime_error("Failed to initialize HarfBuzz font");
    }

    hb_ft_font_set_load_flags(
        m_data->font.get(),
        FT_IS_SCALABLE(m_data->face.get()) ? FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP : FT_LOAD_DEFAULT
    );
}

font_face::~font_face() = default;

std::string font_face::family() const
{
    return state().face->family_name ? state().face->family_name : "";
}

long font_face::face_count() const
{
    return state().face->num_faces;
}

int font_face::weight() const
{
    const auto* table = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(state().face.get(), ft_sfnt_os2));

    return table && table->usWeightClass                      ? table->usWeightClass
           : (state().face->style_flags & FT_STYLE_FLAG_BOLD) ? 700
                                                              : 400;
}

font_slant font_face::slant() const
{
    return (state().face->style_flags & FT_STYLE_FLAG_ITALIC) ? font_slant::italic : font_slant::normal;
}

bool font_face::covers(std::uint32_t scalar) const
{
    return FT_Get_Char_Index(state().face.get(), scalar) != 0;
}

bool font_face::variant(std::uint32_t scalar, std::uint32_t selector) const
{
    return FT_Face_GetCharVariantIndex(state().face.get(), scalar, selector) != 0;
}

bool font_face::color() const
{
    return FT_HAS_COLOR(state().face.get());
}

bool font_face::variable() const
{
    return FT_HAS_MULTIPLE_MASTERS(state().face.get());
}

long font_face::index() const
{
    return state().face->face_index;
}

std::size_t font_face::storage_bytes() const
{
    return state().bytes->size();
}

std::shared_ptr<const std::vector<unsigned char>> font_face::bytes() const
{
    return state().bytes;
}

font_face::data& font_face::state() const
{
    if(!m_data)
    {
        throw std::logic_error("Font is moved from");
    }

    return *m_data;
}

std::shared_ptr<const void> font_face::identity() const
{
    return state().identity;
}

layout_data font_face::shape_run(
    std::string_view text,
    float size,
    bool rtl,
    std::uint32_t script,
    std::string_view language,
    std::size_t begin,
    std::size_t length
)
{
    validate_text(text);
    state().set_size(size);

    if(text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::length_error("Text exceeds HarfBuzz input limits");
    }

    hb_ft_font_changed(m_data->font.get());

    std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer{hb_buffer_create(), hb_buffer_destroy};

    if(!buffer)
    {
        throw std::runtime_error("Failed to create HarfBuzz buffer");
    }

    if(begin > text.size())
    {
        throw std::out_of_range("Shaping range is outside text");
    }

    length = std::min(length, text.size() - begin);
    hb_buffer_add_utf8(
        buffer.get(),
        text.data(),
        static_cast<int>(text.size()),
        static_cast<unsigned>(begin),
        static_cast<int>(length)
    );
    if(script)
    {
        hb_buffer_set_direction(buffer.get(), rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer.get(), hb_script_from_iso15924_tag(script));
    }
    if(!language.empty())
    {
        hb_buffer_set_language(
            buffer.get(),
            hb_language_from_string(language.data(), static_cast<int>(language.size()))
        );
    }

    hb_buffer_set_cluster_level(buffer.get(), HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
    hb_buffer_guess_segment_properties(buffer.get());

    hb_shape(m_data->font.get(), buffer.get(), nullptr, 0);
    if(!hb_buffer_allocation_successful(buffer.get()))
    {
        throw std::bad_alloc{};
    }

    unsigned int count = 0;
    const auto infos = std::span{hb_buffer_get_glyph_infos(buffer.get(), &count), count};
    const auto positions = std::span{hb_buffer_get_glyph_positions(buffer.get(), nullptr), count};

    layout_data result;
    result.glyphs.reserve(count);

    float x = 0.0f;
    float y = 0.0f;
    const auto unit = m_data->size_scale / 64.0f;
    bool has_ink = false;

    for(unsigned int i = 0; i < count; ++i)
    {
        result.glyphs.push_back(
            {infos[i].codepoint, x + positions[i].x_offset * unit, y + positions[i].y_offset * unit, infos[i].cluster}
        );
        result.glyphs.back().advance = positions[i].x_advance * unit;
        result.glyphs.back().origin = x;

        hb_glyph_extents_t extents{};
        if(hb_font_get_glyph_extents(m_data->font.get(), infos[i].codepoint, &extents) && extents.width != 0 &&
           extents.height != 0)
        {
            const auto left = result.glyphs.back().x + extents.x_bearing / 64.0f;
            const auto top = -(result.glyphs.back().y + extents.y_bearing / 64.0f);
            const rect ink{left, top, extents.width / 64.0f, -extents.height / 64.0f};
            result.glyphs.back().ink =
                {extents.x_bearing * unit, -extents.y_bearing * unit, extents.width * unit, -extents.height * unit};

            if(!has_ink)
            {
                result.ink_bounds = ink;
                has_ink = true;
            }
            else
            {
                const auto right = std::max(result.ink_bounds.x + result.ink_bounds.width, ink.x + ink.width);
                const auto bottom = std::max(result.ink_bounds.y + result.ink_bounds.height, ink.y + ink.height);
                result.ink_bounds.x = std::min(result.ink_bounds.x, ink.x);
                result.ink_bounds.y = std::min(result.ink_bounds.y, ink.y);
                result.ink_bounds.width = right - result.ink_bounds.x;
                result.ink_bounds.height = bottom - result.ink_bounds.y;
            }
        }

        x += positions[i].x_advance * unit;
        y += positions[i].y_advance * unit;
    }

    result.bounds.width = x;
    result.bounds.height = FT_IS_SCALABLE(m_data->face.get())
                               ? FT_MulFix(m_data->face->height, m_data->face->size->metrics.y_scale) * unit
                               : m_data->face->size->metrics.height * unit;
    result.ascender = FT_IS_SCALABLE(m_data->face.get())
                          ? FT_MulFix(m_data->face->ascender, m_data->face->size->metrics.y_scale) * unit
                          : m_data->face->size->metrics.ascender * unit;

    return result;
}

glyph_bitmap font_face::rasterize(std::uint32_t id, float size)
{
    state().set_size(size);

    const auto load = [&](std::uint32_t glyph, FT_Int32 flags)
    {
        if(FT_Load_Glyph(m_data->face.get(), glyph, flags))
        {
            return false;
        }

        if(m_data->face->glyph->format != FT_GLYPH_FORMAT_BITMAP &&
           FT_Render_Glyph(m_data->face->glyph, FT_RENDER_MODE_NORMAL))
        {
            return false;
        }

        const auto mode = m_data->face->glyph->bitmap.pixel_mode;

        return mode == FT_PIXEL_MODE_GRAY || mode == FT_PIXEL_MODE_BGRA || mode == FT_PIXEL_MODE_MONO ||
               m_data->face->glyph->bitmap.width == 0 || m_data->face->glyph->bitmap.rows == 0;
    };

    if(!load(id, FT_LOAD_DEFAULT | FT_LOAD_COLOR) && !load(id, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP) &&
       !load(0, FT_LOAD_DEFAULT))
    {
        throw std::runtime_error("Failed to rasterize glyph or missing-glyph fallback");
    }

    const auto& slot = *m_data->face->glyph;
    const auto& bitmap = slot.bitmap;

    auto result = copy_glyph_bitmap(bitmap, slot.bitmap_left, slot.bitmap_top);
    const auto channels = result.format == pixel_format::rgba ? std::size_t{4} : std::size_t{1};

    if(m_data->size_scale != 1 && !result.pixels.empty())
    {
        const auto factor = m_data->size_scale;
        const auto scaled_width = std::round(static_cast<double>(result.width) * factor);
        const auto scaled_height = std::round(static_cast<double>(result.height) * factor);
        if(scaled_width > std::numeric_limits<int>::max() || scaled_height > std::numeric_limits<int>::max() ||
           std::abs(static_cast<double>(result.left) * factor) > std::numeric_limits<int>::max() ||
           std::abs(static_cast<double>(result.top) * factor) > std::numeric_limits<int>::max() ||
           std::max(1.0, scaled_width) * std::max(1.0, scaled_height) >
               std::numeric_limits<std::size_t>::max() / channels)
        {
            throw std::length_error("Scaled glyph is too large");
        }

        glyph_bitmap scaled{
            std::max(1, static_cast<int>(scaled_width)),
            std::max(1, static_cast<int>(scaled_height)),
            static_cast<int>(std::round(result.left * factor)),
            static_cast<int>(std::round(result.top * factor)),
            {}
        };
        scaled.format = result.format;
        scaled.pixels.resize(static_cast<std::size_t>(scaled.width) * scaled.height * channels);

        for(int y = 0; y < scaled.height; ++y)
        {
            const auto source_y = std::min(result.height - 1, static_cast<int>(y / factor));
            for(int x = 0; x < scaled.width; ++x)
            {
                const auto source_x = std::min(result.width - 1, static_cast<int>(x / factor));
                for(std::size_t channel = 0; channel < channels; ++channel)
                {
                    scaled.pixels[(static_cast<std::size_t>(y) * scaled.width + x) * channels + channel] =
                        result.pixels
                            [(static_cast<std::size_t>(source_y) * result.width + source_x) * channels + channel];
                }
            }
        }

        return scaled;
    }

    return result;
}

std::vector<float> font_face::ligature_carets(std::uint32_t glyph, float size, bool rtl)
{
    state().set_size(size);
    hb_ft_font_changed(m_data->font.get());
    unsigned count = 0;
    const auto total = hb_ot_layout_get_ligature_carets(
        m_data->font.get(),
        rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR,
        glyph,
        0,
        &count,
        nullptr
    );

    std::vector<hb_position_t> positions(total);
    count = total;
    hb_ot_layout_get_ligature_carets(
        m_data->font.get(),
        rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR,
        glyph,
        0,
        &count,
        positions.data()
    );

    std::vector<float> result;
    for(unsigned i = 0; i < count; ++i)
    {
        result.push_back(std::abs(positions[i] * m_data->size_scale / 64.0f));
    }

    std::ranges::sort(result);

    return result;
}

} // namespace centypo::details

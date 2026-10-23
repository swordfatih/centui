#include <span>

#include <centui/font.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

namespace centui
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

struct font::data
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

    // Reverse destruction: HarfBuzz, face, then FreeType library.
    std::unique_ptr<std::remove_pointer_t<FT_Library>, library_deleter> library;
    std::unique_ptr<std::remove_pointer_t<FT_Face>, face_deleter> face;
    std::unique_ptr<hb_font_t, font_deleter> font;

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

font::font(const std::filesystem::path& path)
    : m_data(std::make_shared<data>())
{
    if(FT_Init_FreeType(output_handle{m_data->library}))
    {
        throw std::runtime_error("Failed to initialize FreeType");
    }

    if(FT_New_Face(m_data->library.get(), path.string().c_str(), 0, output_handle{m_data->face}))
    {
        throw std::runtime_error("Failed to load font: " + path.string());
    }

    if(FT_Set_Pixel_Sizes(m_data->face.get(), 0, 16))
    {
        throw std::runtime_error("Failed to set font size");
    }

    m_data->font.reset(hb_ft_font_create_referenced(m_data->face.get()));

    if(!m_data->font)
    {
        throw std::runtime_error("Failed to initialize HarfBuzz font");
    }
}

font::~font() = default;
font::font(const font&) = default;
font& font::operator=(const font&) = default;

font::font(font&&) noexcept = default;
font& font::operator=(font&&) noexcept = default;

font::data& font::state() const
{
    if(!m_data)
    {
        throw std::logic_error("Font is moved from");
    }

    return *m_data;
}

std::shared_ptr<const void> font::identity() const
{
    return state().identity;
}

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

    hb_buffer_add_utf8(buffer.get(), text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_buffer_guess_segment_properties(buffer.get());
    hb_shape(m_data->font.get(), buffer.get(), nullptr, 0);

    unsigned int count = 0;
    const auto infos = std::span{hb_buffer_get_glyph_infos(buffer.get(), &count), count};
    const auto positions = std::span{hb_buffer_get_glyph_positions(buffer.get(), nullptr), count};

    text_layout result;
    result.glyphs.reserve(count);

    float x = 0.0f;
    float y = 0.0f;

    for(unsigned int i = 0; i < count; ++i)
    {
        result.glyphs.push_back(
            {infos[i].codepoint, x + positions[i].x_offset / 64.0f, y + positions[i].y_offset / 64.0f, infos[i].cluster}
        );
        result.glyphs.back().advance = positions[i].x_advance / 64.0f;
        result.glyphs.back().origin = x;

        x += positions[i].x_advance / 64.0f;
        y += positions[i].y_advance / 64.0f;
    }

    result.bounds.width = x;
    result.bounds.height = m_data->face->size->metrics.height / 64.0f;
    result.ascender = m_data->face->size->metrics.ascender / 64.0f;

    return result;
}

glyph_bitmap font::rasterize(std::uint32_t id, float size)
{
    state().set_size(size);

    if(FT_Load_Glyph(m_data->face.get(), id, FT_LOAD_DEFAULT))
    {
        throw std::runtime_error("Failed to load glyph");
    }

    if(FT_Render_Glyph(m_data->face->glyph, FT_RENDER_MODE_NORMAL))
    {
        throw std::runtime_error("Failed to rasterize glyph");
    }

    const auto& slot = *m_data->face->glyph;
    const auto& bitmap = slot.bitmap;

    if(bitmap.pixel_mode != FT_PIXEL_MODE_GRAY && bitmap.width > 0 && bitmap.rows > 0)
    {
        throw std::runtime_error("Unsupported glyph bitmap format");
    }

    if(bitmap.width > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
       bitmap.rows > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
       (bitmap.width != 0 && bitmap.rows > std::numeric_limits<std::size_t>::max() / bitmap.width))
    {
        throw std::length_error("Glyph bitmap dimensions are not representable");
    }

    glyph_bitmap result{
        static_cast<int>(bitmap.width),
        static_cast<int>(bitmap.rows),
        slot.bitmap_left,
        slot.bitmap_top,
        {},
    };

    result.pixels.resize(static_cast<std::size_t>(result.width) * result.height);

    for(int row = 0; row < result.height; ++row)
    {
        const auto pitch = static_cast<std::ptrdiff_t>(bitmap.pitch);
        const auto offset = pitch >= 0 ? row * pitch : (result.height - 1 - row) * -pitch;
        const auto source = std::span{bitmap.buffer + offset, static_cast<std::size_t>(result.width)};
        const auto destination =
            std::span{result.pixels}.subspan(static_cast<std::size_t>(row) * result.width, result.width);

        std::ranges::copy(source, destination.begin());
    }

    return result;
}

float font::ascender(float size)
{
    state().set_size(size);

    return m_data->face->size->metrics.ascender / 64.0f;
}

text_service::text_service()
    : m_data(std::make_shared<data>())
{
}

void text_service::set_scale(float value)
{
    if(!std::isfinite(value) || value <= 0)
    {
        throw std::invalid_argument("Text scale must be finite and positive");
    }

    m_scale = value;
}

float text_service::scale() const noexcept
{
    return m_scale;
}

text_service::text_service(const centui::font& face)
    : text_service()
{
    set_font(face);
}

void text_service::set_font(const centui::font& face)
{
    (void)face.identity();

    if(m_data->default_font == face)
    {
        return;
    }

    auto revision = std::make_shared<const int>(0);
    m_data->default_font = face;
    m_data->revision = std::move(revision);
}

std::shared_ptr<const void> text_service::identity() const
{
    return m_data->revision;
}

std::shared_ptr<const void> text_service::font_identity(const std::optional<centui::font>& face) const
{
    return resolve(face).identity();
}

void text_service::clear_cache()
{
    m_data->images.clear();
    m_data->cache_bytes = 0;
}

void text_service::set_cache_budget(std::size_t bytes)
{
    m_data->cache_budget = bytes;
    clear_cache();
}

text_layout text_service::layout_text(
    std::string_view value,
    float size,
    float width,
    const std::optional<centui::font>& face
) const
{
    auto selected = resolve(face);

    if(std::isnan(width) || width < 0)
    {
        throw std::invalid_argument("Text width must be nonnegative");
    }

    if(value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::length_error("Text exceeds HarfBuzz input limits");
    }

    text_layout result;
    result.raster_scale = m_scale;
    const auto metrics = selected.layout_text({}, size);
    result.ascender = metrics.ascender;
    result.line_height = metrics.bounds.height;
    float y = 0;
    const auto append_line = [&](std::string_view line, std::size_t offset)
    {
        auto shaped = selected.layout_text(line, size);
        const auto first = result.glyphs.size();
        result.bounds.width = std::max(result.bounds.width, shaped.bounds.width);
        const bool rtl = shaped.glyphs.size() > 1 && shaped.glyphs.front().cluster > shaped.glyphs.back().cluster;
        result.carets.push_back({offset, {rtl ? shaped.bounds.width : 0, y}});

        for(std::size_t index = 0; index < shaped.glyphs.size();)
        {
            const auto cluster = shaped.glyphs[index].cluster;
            float left = shaped.glyphs[index].origin;
            float right = left;

            while(index < shaped.glyphs.size() && shaped.glyphs[index].cluster == cluster)
            {
                left = std::min(left, shaped.glyphs[index].origin);
                right = std::max(right, shaped.glyphs[index].origin + shaped.glyphs[index].advance);
                ++index;
            }

            result.carets.push_back({offset + cluster, {rtl ? right : left, y}});
        }

        result.carets.push_back({offset + line.size(), {rtl ? 0 : shaped.bounds.width, y}});

        for(auto& glyph: shaped.glyphs)
        {
            glyph.cluster += static_cast<std::uint32_t>(offset);
            glyph.y -= y;
            glyph.image = image(selected, glyph.id, size * m_scale);
            result.glyphs.push_back(std::move(glyph));
        }

        result.lines.push_back({first, result.glyphs.size(), shaped.bounds.width, offset, offset + line.size()});
        y += metrics.bounds.height;
    };

    std::size_t paragraph_begin = 0;

    while(paragraph_begin <= value.size())
    {
        auto end = value.find('\n', paragraph_begin);

        if(end == std::string_view::npos)
        {
            end = value.size();
        }

        auto paragraph = value.substr(paragraph_begin, end - paragraph_begin);

        if(!paragraph.empty() && paragraph.back() == '\r')
        {
            paragraph.remove_suffix(1);
        }

        std::size_t begin = 0;

        if(!paragraph.empty() && (!std::isfinite(width) || selected.measure(paragraph, size).width <= width))
        {
            append_line(paragraph, paragraph_begin);
            begin = paragraph.size();
        }
        while(begin < paragraph.size())
        {
            std::size_t best = begin;
            std::size_t cursor = begin;

            while(cursor <= paragraph.size())
            {
                const auto space = paragraph.find(' ', cursor);
                const auto candidate = space == std::string_view::npos ? paragraph.size() : space;
                const auto bounds = selected.measure(paragraph.substr(begin, candidate - begin), size);

                if(bounds.width > width && best > begin)
                {
                    break;
                }

                best = candidate;

                if(candidate == paragraph.size() || bounds.width > width)
                {
                    break;
                }

                cursor = candidate + 1;
            }

            if(best == begin)
            {
                // Here the byte is an ASCII space, never part of a UTF-8 sequence.
                best = std::min(begin + 1, paragraph.size());
            }

            append_line(paragraph.substr(begin, best - begin), paragraph_begin + begin);
            begin = best;

            if(begin < paragraph.size() && paragraph[begin] == ' ')
            {
                ++begin;
            }
        }

        if(paragraph.empty())
        {
            append_line({}, paragraph_begin);
        }

        if(end == value.size())
        {
            break;
        }

        paragraph_begin = end + 1;
    }

    result.bounds.height = y;

    return result;
}

std::shared_ptr<const text_layout> text_service::prepare(
    std::string_view value,
    const std::optional<centui::font>& face,
    float size,
    float width
) const
{
    return std::make_shared<const text_layout>(layout_text(value, size, width, face));
}

rect text_service::measure(std::string_view value, float size) const
{
    return layout_text(value, size).bounds;
}

const centui::font& text_service::resolve(const std::optional<centui::font>& face) const
{
    const auto& selected = face ? face : m_data->default_font;

    if(!selected)
    {
        throw std::logic_error("Text requires a font; set the document font or style.font");
    }

    (void)selected->identity();

    return *selected;
}

std::shared_ptr<const glyph_bitmap> text_service::image(centui::font& face, std::uint32_t glyph, float size) const
{
    const auto identity = face.identity();
    const auto key = std::pair{size, glyph};

    if(const auto images = m_data->images.find(identity); images != m_data->images.end())
    {
        if(const auto found = images->second.find(key); found != images->second.end())
        {
            return found->second;
        }
    }

    auto output = std::make_shared<const glyph_bitmap>(face.rasterize(glyph, size));
    constexpr auto overhead = sizeof(glyph_bitmap) + 128;

    if(m_data->cache_budget < overhead || output->pixels.size() > m_data->cache_budget - overhead)
    {
        return output;
    }

    const auto bytes = output->pixels.size() + overhead;

    if(m_data->cache_bytes > m_data->cache_budget - bytes)
    {
        // Cached ownership is bounded; live draw lists retain their immutable images.
        m_data->images.clear();
        m_data->cache_bytes = 0;
    }

    m_data->images[identity].emplace(key, output);
    m_data->cache_bytes += bytes;

    return output;
}

} // namespace centui

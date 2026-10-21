module;

#include <algorithm>
#include <atomic>
#include <map>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <utility>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

export module centui.font;

export import centui.render;

export namespace centui
{

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

    // Identifies the font data across moves without retaining native resources.
    [[nodiscard]] std::shared_ptr<const void> identity() const;

    // Share native data with CPU preparation without changing move-only handles.
    [[nodiscard]] font share() const
    {
        (void)state();
        return font{_data};
    }

private:
    struct data;
    [[nodiscard]] data& state() const;
    explicit font(std::shared_ptr<data> value) : _data(std::move(value))
    {
    }

    std::shared_ptr<data> _data;
};

} // namespace centui

namespace centui
{

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
#elif defined(__APPLE__)
    for(const auto* path: {"/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/Helvetica.ttc"})
    {
        if(std::filesystem::exists(path))
        {
            return path;
        }
    }
#else
    for(const auto* path: {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                          "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                          "/usr/share/fonts/TTF/DejaVuSans.ttf"})
    {
        if(std::filesystem::exists(path))
        {
            return path;
        }
    }
#endif
    throw std::runtime_error("No default system font found; supply a font file explicitly");
}

} // namespace details

font::font() : font(details::default_font_path())
{
}

font::font(const std::filesystem::path& path) : _data(std::make_shared<data>())
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

font::data& font::state() const
{
    if(!_data)
    {
        throw std::logic_error("Font is moved from");
    }
    return *_data;
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
                                 y + positions[i].y_offset / 64.0f,
                                 infos[i].cluster});

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
    state().set_size(size);

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
        {}};

    result.pixels.resize(static_cast<std::size_t>(result.width) * result.height);

    for(int row = 0; row < result.height; ++row)
    {
        const auto pitch = static_cast<std::ptrdiff_t>(bitmap.pitch);
        const auto offset = pitch >= 0 ? row * pitch : (result.height - 1 - row) * -pitch;
        const auto* source = bitmap.buffer + offset;
        std::copy_n(source, result.width, result.pixels.data() + static_cast<std::size_t>(row) * result.width);
    }

    return result;
}

float font::ascender(float size)
{
    state().set_size(size);

    return _data->face->size->metrics.ascender / 64.0f;
}

} // namespace centui



export namespace centui
{

// Copies share a single-threaded CPU service. No window or graphics context.
class text_service
{
    struct font_entry
    {
        centui::font face;
        std::map<std::pair<float, std::uint32_t>, std::shared_ptr<const glyph_bitmap>> images;
    };

    struct data
    {
        std::shared_ptr<const void> revision = std::make_shared<const int>(0);
        std::map<std::uint64_t, font_entry> fonts;
        font_handle default_font;
        font_handle shared_font;
        bool system_default = true;
        std::size_t cache_budget = 8 * 1024 * 1024;
        std::size_t cache_bytes = 0;
    };

public:
    text_service() : _data(std::make_shared<data>())
    {
    }

    explicit text_service(const centui::font& face) : text_service()
    {
        set_default_font(face);
    }

    [[nodiscard]] font_handle load_font(const std::filesystem::path& path)
    {
        return add_font(centui::font{path});
    }

    [[nodiscard]] font_handle add_font(centui::font face)
    {
        (void)face.identity();
        static std::atomic<std::uint64_t> next{1};
        auto id = next.load(std::memory_order_relaxed);
        do
        {
            if(id == std::numeric_limits<std::uint64_t>::max())
            {
                throw std::overflow_error("Font identity space exhausted");
            }
        } while(!next.compare_exchange_weak(id, id + 1, std::memory_order_relaxed));

        auto revision = std::make_shared<const int>(0);
        _data->fonts.emplace(id, font_entry{std::move(face), {}});
        _data->revision = std::move(revision);
        return {id};
    }

    void set_default_font(font_handle handle)
    {
        (void)find(handle, false);
        if(_data->default_font == handle)
        {
            return;
        }
        auto revision = std::make_shared<const int>(0);
        _data->default_font = handle;
        _data->revision = std::move(revision);
    }

    void set_default_font(const centui::font& face)
    {
        const auto identity = face.identity();
        if(_data->default_font.value != 0 && find(_data->default_font).face.identity() == identity)
        {
            return;
        }
        const auto previous = _data->shared_font;
        const auto handle = add_font(face.share());
        set_default_font(handle);
        if(previous.value != 0)
        {
            remove_font(previous);
        }
        _data->shared_font = handle;
    }

    // Load replacement data under a new handle; existing styles opt in explicitly.
    void remove_font(font_handle handle)
    {
        (void)find(handle, false);
        auto revision = std::make_shared<const int>(0);
        clear_cache();
        _data->fonts.erase(handle.value);
        if(_data->default_font == handle)
        {
            _data->default_font = {};
            _data->system_default = false;
        }
        if(_data->shared_font == handle)
        {
            _data->shared_font = {};
        }
        _data->revision = std::move(revision);
    }

    [[nodiscard]] std::shared_ptr<const void> identity() const
    {
        return _data->revision;
    }

    [[nodiscard]] std::shared_ptr<const void> font_identity(font_handle handle = {}) const
    {
        return find(handle).face.identity();
    }

    void clear_cache()
    {
        for(auto& [id, entry]: _data->fonts)
        {
            entry.images.clear();
        }
        _data->cache_bytes = 0;
    }

    void set_cache_budget(std::size_t bytes)
    {
        _data->cache_budget = bytes;
        clear_cache();
    }

    // Newlines and space-delimited word wrapping. Long words overflow intact.
    // Shape selected lines again, preserving contextual shaping at line edges.
    [[nodiscard]] text_layout layout_text(std::string_view value, float size,
                                          float width = std::numeric_limits<float>::infinity(),
                                          font_handle handle = {}) const
    {
        auto& entry = find(handle);
        if(std::isnan(width) || width < 0)
        {
            throw std::invalid_argument("Text width must be nonnegative");
        }
        if(value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("Text exceeds HarfBuzz input limits");
        }

        text_layout result;
        const auto metrics = entry.face.layout_text({}, size);
        result.ascender = metrics.ascender;
        float y = 0;
        const auto append_line = [&](std::string_view line, std::size_t offset)
        {
            auto shaped = entry.face.layout_text(line, size);
            const auto first = result.glyphs.size();
            result.bounds.width = std::max(result.bounds.width, shaped.bounds.width);
            for(auto& glyph: shaped.glyphs)
            {
                glyph.cluster += static_cast<std::uint32_t>(offset);
                glyph.y -= y;
                glyph.image = image(entry, glyph.id, size);
                result.glyphs.push_back(std::move(glyph));
            }
            result.lines.push_back({first, result.glyphs.size(), shaped.bounds.width});
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
            if(!paragraph.empty() && (!std::isfinite(width) || entry.face.measure(paragraph, size).width <= width))
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
                    const auto bounds = entry.face.measure(paragraph.substr(begin, candidate - begin), size);
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

    // The caller may retain this layout across preparations and cache eviction.
    [[nodiscard]] std::shared_ptr<const text_layout> prepare(
        std::string_view value, font_handle handle, float size,
        float width = std::numeric_limits<float>::infinity()) const
    {
        return std::make_shared<const text_layout>(layout_text(value, size, width, handle));
    }

    [[nodiscard]] rect measure(std::string_view value, float size) const
    {
        return layout_text(value, size).bounds;
    }

private:
    font_entry& find(font_handle handle, bool use_default = true) const
    {
        if(use_default && handle.value == 0)
        {
            if(_data->default_font.value == 0 && _data->system_default)
            {
                auto shared = *this;
                shared.set_default_font(centui::font{});
            }
            handle = _data->default_font;
        }
        const auto found = _data->fonts.find(handle.value);
        if(found == _data->fonts.end())
        {
            throw std::invalid_argument("Font is unavailable in this text service");
        }
        return found->second;
    }

    std::shared_ptr<const glyph_bitmap> image(font_entry& entry, std::uint32_t glyph, float size) const
    {
        const auto key = std::pair{size, glyph};
        if(const auto found = entry.images.find(key); found != entry.images.end())
        {
            return found->second;
        }
        auto output = std::make_shared<const glyph_bitmap>(entry.face.rasterize(glyph, size));
        constexpr auto overhead = sizeof(glyph_bitmap) + 128;
        if(_data->cache_budget < overhead || output->pixels.size() > _data->cache_budget - overhead)
        {
            return output;
        }
        const auto bytes = output->pixels.size() + overhead;
        if(_data->cache_bytes > _data->cache_budget - bytes)
        {
            // Cached ownership is bounded; live draw lists retain their immutable images.
            for(auto& [id, current]: _data->fonts)
            {
                current.images.clear();
            }
            _data->cache_bytes = 0;
        }
        entry.images.emplace(key, output);
        _data->cache_bytes += bytes;
        return output;
    }

    std::shared_ptr<data> _data;
};

} // namespace centui


#include "text_engine.hpp"

#include "text_layout.hpp"

#include <iomanip>
#include <sstream>

namespace centypo::details
{
namespace
{

std::size_t layout_bytes(const layout_data& value)
{
    return sizeof(value) + value.glyphs.capacity() * sizeof(glyph) +
           value.lines.capacity() * sizeof(layout_data::line) + value.carets.capacity() * sizeof(layout_data::caret) +
           value.selection_cells.capacity() * sizeof(layout_data::selection_cell);
}

std::string options_key(const font& request, float size, float width, const text_options& options)
{
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << size << ' ' << width << ' ' << request.weight
        << ' ' << static_cast<int>(request.slant) << ' ' << request.families.size() << ' ';
    for(const auto& family: request.families)
    {
        out << std::quoted(family) << ' ';
    }

    out << static_cast<int>(options.wrap) << ' ' << static_cast<int>(options.direction) << ' '
        << std::quoted(options.language) << ' ' << options.line_spacing << ' ' << options.max_lines << ' '
        << options.ellipsis << ' ';
    for(const auto stop: options.tab_stops)
    {
        out << stop << ' ';
    }

    // Alignment is placement, and intentionally absent from the shaping/layout key.
    return out.str();
}

} // namespace

text_engine::text_engine()
{
    set_budgets({});
}

void text_engine::track(std::shared_ptr<const void> value, std::size_t bytes)
{
    // Weak bookkeeping is bounded by live resources plus at most 255 expired
    // entries; applications need not query statistics to reclaim it.
    if(m_retained.size() % 256 == 0)
    {
        std::erase_if(
            m_retained,
            [](const auto& entry)
            {
                return entry.value.expired();
            }
        );
    }

    m_retained.push_back({std::move(value), bytes});
}

void text_engine::set_budgets(text_cache_budgets budgets)
{
    m_analysis.set_budget(budgets.shaping / 2);
    m_shapes.set_budget(budgets.shaping - budgets.shaping / 2);
    m_layouts.set_budget(budgets.layouts);
    m_images.set_budget(budgets.images);
    m_raster_budget = std::min(budgets.layouts, budgets.images);
}

void text_engine::clear()
{
    m_analysis.clear();
    m_shapes.clear();
    m_layouts.clear();
    m_images.clear();
}

std::shared_ptr<const text_boundaries> text_engine::analyze(std::string_view text, std::string_view language)
{
    return paragraphs(text, language, text_direction::automatic);
}

std::shared_ptr<const text_analysis> text_engine::paragraphs(
    std::string_view text,
    std::string_view language,
    text_direction direction
)
{
    const analysis_key key{std::string(text), std::string(language), direction};
    if(auto hit = m_analysis.find(key))
    {
        return hit;
    }

    auto result = std::make_shared<const text_analysis>(text, language, direction);
    m_analysis.insert(key, result, result->storage_bytes() + 2 * (text.size() + language.size()) + 256);

    return result;
}

std::shared_ptr<const layout_data> text_engine::shape(
    std::shared_ptr<font_face> face,
    std::string_view text,
    float size,
    bool rtl,
    std::uint32_t script,
    std::string_view language,
    std::size_t begin,
    std::size_t length
)
{
    std::erase_if(
        m_fonts,
        [](const auto& entry)
        {
            return entry.first.expired();
        }
    );
    m_fonts[face->bytes()] = face->storage_bytes();

    length = std::min(length, text.size() - std::min(begin, text.size()));
    const shape_key key{face->identity(), std::string(text), size, rtl, script, std::string(language), begin, length};

    if(auto hit = m_shapes.find(key))
    {
        ++m_statistics.shaping_hits;
        return hit;
    }

    auto result = std::make_shared<layout_data>(face->shape_run(text, size, rtl, script, language, begin, length));
    for(auto& glyph: result->glyphs)
    {
        glyph_access::face(glyph) = face;
    }

    ++m_statistics.shapes;
    const auto bytes = layout_bytes(*result);
    m_shapes.insert(key, result, bytes + 2 * (text.size() + language.size()) + 384);
    track(result, bytes);

    return result;
}

std::shared_ptr<const layout_data> text_engine::layout(
    std::shared_ptr<font_collection> collection,
    std::string_view text,
    const font& request,
    float size,
    float width,
    const text_options& options,
    text_alignment alignment
)
{
    request.validate();
    options.validate();
    validate_text(text);
    if(!std::isfinite(size) || size <= 0 || std::isnan(width) || width < 0 || alignment < text_alignment::left ||
       alignment > text_alignment::end)
    {
        throw std::invalid_argument("Invalid text constraints");
    }

    const layout_key key{collection, std::string(text), options_key(request, size, width, options)};
    auto result = m_layouts.find(key);

    if(result)
    {
        ++m_statistics.layout_hits;
    }
    else
    {
        result = std::make_shared<const layout_data>(
            layout_paragraphs(*this, collection, request, text, size, width, options)
        );
        const auto bytes = layout_bytes(*result);
        m_layouts.insert(key, result, bytes + 2 * (text.size() + std::get<2>(key).size()) + 256);
        track(result, bytes);
    }

    if(alignment != text_alignment::left)
    {
        return place(std::move(result), width, alignment);
    }

    return result;
}

std::shared_ptr<const layout_data> text_engine::place(
    std::shared_ptr<const layout_data> layout,
    float width,
    text_alignment alignment
)
{
    const placement_key key{{layout}, width, alignment};
    if(auto hit = m_layouts.find(key))
    {
        return hit;
    }

    auto result = std::make_shared<layout_data>(*layout);
    place_text(*result, width, alignment);

    const auto bytes = layout_bytes(*result);
    m_layouts.insert(key, result, bytes + 2 * sizeof(prepared_key) + 128);
    track(result, bytes);

    return result;
}

std::shared_ptr<const glyph_bitmap> text_engine::image(std::shared_ptr<font_face> face, std::uint32_t glyph, float size)
{
    m_fonts[face->bytes()] = face->storage_bytes();
    const image_key key{face->identity(), glyph, size};

    if(auto hit = m_images.find(key))
    {
        return hit;
    }

    auto result = std::make_shared<const glyph_bitmap>(face->rasterize(glyph, size));
    const auto bytes = sizeof(glyph_bitmap) + result->pixels.capacity();
    m_images.insert(key, result, bytes + 192);
    track(result, bytes);

    return result;
}

std::shared_ptr<const layout_data> text_engine::rasterize(std::shared_ptr<const layout_data> layout, float scale)
{
    if(layout->rasterized && layout->raster_scale == scale)
    {
        return layout;
    }

    const raster_key key{{layout}, scale};
    if(auto hit = m_layouts.find(key))
    {
        return hit;
    }

    auto result = std::make_shared<layout_data>(*layout);
    result->raster_scale = scale;
    result->rasterized = true;

    const auto bytes = layout_bytes(*result);
    auto reservation = bytes + 2 * sizeof(prepared_key) + 128;
    bool cacheable = m_raster_budget > 0 && reservation <= m_raster_budget;

    for(auto& glyph: result->glyphs)
    {
        if(!glyph_access::face(glyph))
        {
            throw std::invalid_argument("Logical glyph has no retained face");
        }

        glyph.image = image(glyph_access::face(glyph), glyph.id, layout->font_size * scale);

        // Prepared layouts also retain their pixels. Reserve those bytes here
        // so evicting the glyph-image cache cannot bypass the memory budget.
        if(cacheable)
        {
            const auto image_bytes = sizeof(glyph_bitmap) + glyph.image->pixels.capacity();
            if(image_bytes > m_raster_budget - reservation)
            {
                cacheable = false;
            }
            else
            {
                reservation += image_bytes;
            }
        }
    }

    if(cacheable)
    {
        m_layouts.insert(key, result, reservation);
    }
    track(result, bytes);

    return result;
}

text_cache_statistics text_engine::statistics(const font_collection& collection) const
{
    auto result = m_statistics;
    result.shaping_bytes = m_analysis.bytes() + m_shapes.bytes();
    result.layout_bytes = m_layouts.bytes();
    result.image_bytes = m_images.bytes();

    std::erase_if(
        m_fonts,
        [](const auto& entry)
        {
            return entry.first.expired();
        }
    );
    for(const auto& bytes: collection.loaded_bytes())
    {
        m_fonts[bytes] = bytes->size();
    }

    for(const auto& [bytes, count]: m_fonts)
    {
        result.font_bytes += count;
    }

    std::erase_if(
        m_retained,
        [](const auto& resource)
        {
            return resource.value.expired();
        }
    );
    for(const auto& resource: m_retained)
    {
        if(const auto live = resource.value.lock())
        {
            const bool cached = m_shapes.owns(live.get()) || m_layouts.owns(live.get()) || m_images.owns(live.get());
            if(!cached || live.use_count() > 2)
            {
                result.retained_bytes += resource.bytes;
            }
        }
    }

    return result;
}

} // namespace centypo::details

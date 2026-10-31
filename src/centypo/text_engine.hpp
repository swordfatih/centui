#pragma once

#include "font_collection.hpp"
#include "text_analysis.hpp"
#include "text_cache.hpp"

#include <tuple>
#include <variant>

namespace centypo::details
{

class text_engine
{
public:
    text_engine();

    void set_budgets(text_cache_budgets budgets);
    void clear();

    std::shared_ptr<const text_boundaries> analyze(std::string_view text, std::string_view language);

    std::shared_ptr<const text_analysis> paragraphs(
        std::string_view text,
        std::string_view language,
        text_direction direction
    );

    std::shared_ptr<const layout_data> shape(
        std::shared_ptr<font_face> face,
        std::string_view text,
        float size,
        bool rtl,
        std::uint32_t script,
        std::string_view language,
        std::size_t begin = 0,
        std::size_t length = std::string_view::npos
    );

    std::shared_ptr<const layout_data> layout(
        std::shared_ptr<font_collection> collection,
        std::string_view text,
        const font& request,
        float size,
        float width,
        const text_options& options,
        text_alignment alignment
    );

    std::shared_ptr<const glyph_bitmap> image(std::shared_ptr<font_face> face, std::uint32_t glyph, float size);

    std::shared_ptr<const layout_data> rasterize(std::shared_ptr<const layout_data> layout, float scale);
    std::shared_ptr<const layout_data> place(
        std::shared_ptr<const layout_data> layout,
        float width,
        text_alignment alignment
    );

    text_cache_statistics statistics(const font_collection& collection) const;

private:
    using analysis_key = std::tuple<std::string, std::string, text_direction>;

    using shape_key = std::tuple<
        std::shared_ptr<const void>,
        std::string,
        float,
        bool,
        std::uint32_t,
        std::string,
        std::size_t,
        std::size_t>;

    using layout_key = std::tuple<std::shared_ptr<font_collection>, std::string, std::string>;
    using image_key = std::tuple<std::shared_ptr<const void>, std::uint32_t, float>;

    // Weak ownership identifies immutable input storage without retaining an
    // unbudgeted copy. Control-block identity also prevents address reuse hits.
    struct layout_reference
    {
        std::weak_ptr<const layout_data> value;

        bool operator<(const layout_reference& other) const noexcept
        {
            return std::owner_less<>{}(value, other.value);
        }
    };

    using placement_key = std::tuple<layout_reference, float, text_alignment>;
    using raster_key = std::tuple<layout_reference, float>;
    using prepared_key = std::variant<layout_key, placement_key, raster_key>;

    resource_cache<analysis_key, text_analysis> m_analysis;
    resource_cache<shape_key, layout_data> m_shapes;
    resource_cache<prepared_key, layout_data> m_layouts;
    resource_cache<image_key, glyph_bitmap> m_images;

    std::size_t m_raster_budget = 0;

    text_cache_statistics m_statistics;

    struct retained_resource
    {
        std::weak_ptr<const void> value;
        std::size_t bytes;
    };

    mutable std::vector<retained_resource> m_retained;

    void track(std::shared_ptr<const void> value, std::size_t bytes);

    mutable std::map<
        std::weak_ptr<const std::vector<unsigned char>>,
        std::size_t,
        std::owner_less<std::weak_ptr<const std::vector<unsigned char>>>>
        m_fonts;
};

} // namespace centypo::details

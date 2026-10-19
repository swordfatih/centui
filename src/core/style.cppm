module;

#include <cmath>
#include <cstdint>
#include <limits>
#include <meta>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

export module centui.style;

import centui.unit;
import centui.color;

export namespace centui
{

// Stages whose inputs changed. Geometry and interaction eligibility are separate.
struct invalidation
{
    bool topology = false;
    bool text = false;
    bool layout = false;
    bool paint = false;
    bool interaction = false;

    constexpr void merge(invalidation other) noexcept
    {
        topology |= other.topology;
        text |= other.text;
        layout |= other.layout;
        paint |= other.paint;
        interaction |= other.interaction;
    }

    constexpr void clear(invalidation completed) noexcept
    {
        topology &= !completed.topology;
        text &= !completed.text;
        layout &= !completed.layout;
        paint &= !completed.paint;
        interaction &= !completed.interaction;
    }

    [[nodiscard]] constexpr bool any() const noexcept { return topology || text || layout || paint || interaction; }
    constexpr bool operator==(const invalidation&) const = default;
};

class style
{
public:
    template <typename... types>
    static consteval style resolve(style output = {})
    {
        (merge(output, types{}), ...);
        output.validate();
        return output;
    }

    edges          padding = edges::all(px(0));
    edges          margin = edges::all(px(0));
    edges          inset = edges::all(fit());
    length         gap = px(0);
    position       position = position::relative;
    length         width = fit();
    length         height = fit();
    direction      flex_direction = direction::column;
    alignment      align_items = alignment::stretch;
    justification  justify_content = justification::start;
    states_color   background_color = states_color::all({0, 0, 0, 0});
    states_color   text_color = states_color::all({255, 255, 255, 255});
    float          font_size = 16.0f;
    text_alignment text_align = text_alignment::left;

    constexpr bool operator==(const style&) const = default;

    // Shared by compile-time construction, full replacement, and runtime patches.
    constexpr void validate() const
    {
        const auto check_length = [](length value, bool nonnegative)
        {
            if(!std::isfinite(value.value)) throw std::invalid_argument("Style lengths must be finite");
            switch(value.unit)
            {
            case unit::px:
            case unit::percent:
                if(nonnegative && value.value < 0) throw std::invalid_argument("Size, padding, and gap cannot be negative");
                break;
            case unit::fit:
                if(value.value != 0) throw std::invalid_argument("Fit lengths must have zero value");
                break;
            default: throw std::invalid_argument("Invalid length unit");
            }
        };
        const auto check_edges = [&](const edges& value, bool nonnegative)
        {
            check_length(value.top, nonnegative);
            check_length(value.right, nonnegative);
            check_length(value.bottom, nonnegative);
            check_length(value.left, nonnegative);
        };
        check_length(width, true);
        check_length(height, true);
        check_length(gap, true);
        check_edges(padding, true);
        check_edges(margin, false);
        check_edges(inset, false);
        if(!std::isfinite(font_size) || font_size <= 0 ||
           std::round(static_cast<double>(font_size)) > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Font size must be positive, finite, and representable");

        const auto check_enum = [](auto value, auto first, auto last)
        {
            if(value < first || value > last) throw std::invalid_argument("Invalid style enum value");
        };
        check_enum(position, centui::position::fixed, centui::position::absolute);
        check_enum(flex_direction, direction::column, direction::row_reverse);
        check_enum(align_items, alignment::automatic, alignment::space_evenly);
        check_enum(justify_content, justification::start, justification::space_evenly);
        check_enum(text_align, text_alignment::left, text_alignment::right);
    }

    [[nodiscard]] constexpr invalidation changes_from(const style& previous) const noexcept
    {
        const bool text_changed = font_size != previous.font_size;
        const bool layout_changed = text_changed || padding != previous.padding || margin != previous.margin ||
            inset != previous.inset || gap != previous.gap || position != previous.position ||
            width != previous.width || height != previous.height || flex_direction != previous.flex_direction ||
            align_items != previous.align_items || justify_content != previous.justify_content;
        return {.text = text_changed,
                .layout = layout_changed,
                .paint = layout_changed || background_color != previous.background_color ||
                         text_color != previous.text_color || text_align != previous.text_align};
    }

private:
    template <typename target_type, typename source_type>
    static consteval void merge(target_type& target, const source_type& source)
    {
        constexpr auto        context = std::meta::access_context::current();
        static constexpr auto target_fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^target_type, context));
        static constexpr auto source_fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^source_type, context));

        template for(constexpr auto target_field: target_fields)
        {
            template for(constexpr auto source_field: source_fields)
            {
                if constexpr(std::meta::identifier_of(target_field) == std::meta::identifier_of(source_field))
                {
                    using target_member = typename[:std::meta::type_of(target_field):];
                    using source_member = typename[:std::meta::type_of(source_field):];

                    if constexpr(std::is_class_v<target_member> && std::is_class_v<source_member> && !std::is_same_v<target_member, length> && !std::is_same_v<target_member, color>)
                    {
                        merge(target.[:target_field:], source.[:source_field:]);
                    }
                    else
                    {
                        target.[:target_field:] = source.[:source_field:];
                    }
                }
            }
        }
    }
};

// Each supplied field replaces that property; unspecified fields are preserved.
struct style_patch
{
    std::optional<edges> padding = std::nullopt;
    std::optional<edges> margin = std::nullopt;
    std::optional<edges> inset = std::nullopt;
    std::optional<length> gap = std::nullopt;
    std::optional<centui::position> position = std::nullopt;
    std::optional<length> width = std::nullopt;
    std::optional<length> height = std::nullopt;
    std::optional<direction> flex_direction = std::nullopt;
    std::optional<alignment> align_items = std::nullopt;
    std::optional<justification> justify_content = std::nullopt;
    std::optional<states_color> background_color = std::nullopt;
    std::optional<states_color> text_color = std::nullopt;
    std::optional<float> font_size = std::nullopt;
    std::optional<text_alignment> text_align = std::nullopt;

    [[nodiscard]] style apply(style candidate) const
    {
        if(padding) candidate.padding = *padding;
        if(margin) candidate.margin = *margin;
        if(inset) candidate.inset = *inset;
        if(gap) candidate.gap = *gap;
        if(position) candidate.position = *position;
        if(width) candidate.width = *width;
        if(height) candidate.height = *height;
        if(flex_direction) candidate.flex_direction = *flex_direction;
        if(align_items) candidate.align_items = *align_items;
        if(justify_content) candidate.justify_content = *justify_content;
        if(background_color) candidate.background_color = *background_color;
        if(text_color) candidate.text_color = *text_color;
        if(font_size) candidate.font_size = *font_size;
        if(text_align) candidate.text_align = *text_align;
        candidate.validate();
        return candidate;
    }
};
} // namespace centui

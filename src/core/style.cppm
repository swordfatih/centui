module;

#include <cmath>
#include <cstdint>
#include <limits>
#include <meta>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

export module centui.style;

export import centui.types;
export import centui.font;

export namespace centui
{

namespace details
{

enum class visual_state
{
    normal,
    hovered,
    pressed,
    focused,
    disabled
};

} // namespace details

struct invalidation
{
    bool layout = false;
    bool paint = false;

    constexpr void merge(invalidation other) noexcept
    {
        layout |= other.layout;
        paint |= other.paint;
    }

    [[nodiscard]] constexpr bool any() const noexcept
    {
        return layout || paint;
    }
    constexpr bool operator==(const invalidation&) const = default;
};

class style
{
public:
    template <typename... types>
    static style resolve(style output = {})
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
    std::optional<centui::font> font = std::nullopt;
    text_alignment text_align = text_alignment::left;
    centui::visibility visibility = centui::visibility::visible;
    centui::overflow overflow = centui::overflow::visible;
    float border_width = 0;
    states_color border_color = states_color::all({0, 0, 0, 0});

    bool operator==(const style&) const = default;

    // Shared by style construction, full replacement, and patches.
    constexpr void validate() const
    {
        const auto check_length = [](length value, bool nonnegative)
        {
            if(!std::isfinite(value.value))
            {
                throw std::invalid_argument("Style lengths must be finite");
            }
            switch(value.unit)
            {
            case unit::px:
            case unit::percent:
            {
                if(nonnegative && value.value < 0)
                {
                    throw std::invalid_argument("Size, padding, and gap cannot be negative");
                }
                break;
            }
            case unit::fit:
            {
                if(value.value != 0)
                {
                    throw std::invalid_argument("Fit lengths must have zero value");
                }
                break;
            }
            default:
            {
                throw std::invalid_argument("Invalid length unit");
            }
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
        {
            throw std::invalid_argument("Font size must be positive, finite, and representable");
        }

        const auto check_enum = [](auto value, auto first, auto last)
        {
            if(value < first || value > last)
            {
                throw std::invalid_argument("Invalid style enum value");
            }
        };
        check_enum(position, centui::position::normal, centui::position::absolute);
        check_enum(flex_direction, direction::column, direction::row_reverse);
        check_enum(align_items, alignment::automatic, alignment::space_evenly);
        check_enum(justify_content, justification::start, justification::space_evenly);
        check_enum(text_align, text_alignment::left, text_alignment::right);
        check_enum(visibility, centui::visibility::visible, centui::visibility::collapsed);
        check_enum(overflow, centui::overflow::visible, centui::overflow::scroll);

        if(!std::isfinite(border_width) || border_width < 0)
        {
            throw std::invalid_argument("Border width must be finite and nonnegative");
        }
    }

    [[nodiscard]] invalidation changes_from(const style& previous) const noexcept
    {
        const bool text_changed = font_size != previous.font_size || font != previous.font;
        const bool layout_changed = text_changed || padding != previous.padding || margin != previous.margin ||
            inset != previous.inset || gap != previous.gap || position != previous.position ||
            width != previous.width || height != previous.height || flex_direction != previous.flex_direction ||
            align_items != previous.align_items || justify_content != previous.justify_content ||
            visibility != previous.visibility || overflow != previous.overflow || border_width != previous.border_width;
        return {.layout = layout_changed,
                .paint = layout_changed || background_color != previous.background_color ||
                         text_color != previous.text_color || text_align != previous.text_align || border_color != previous.border_color};
    }

private:
    static consteval std::meta::info member_named(std::meta::info type, std::string_view name)
    {
        for(const auto member: std::meta::nonstatic_data_members_of(type, std::meta::access_context::current()))
        {
            if(std::meta::identifier_of(member) == name)
            {
                return member;
            }
        }
        throw "Unknown style property";
    }

    template <typename target_type, typename source_type>
    static void merge(target_type& target, const source_type& source)
    {
        constexpr auto context = std::meta::access_context::current();
        static constexpr auto fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^source_type, context));
        template for(constexpr auto source_field: fields)
        {
            constexpr auto target_field = member_named(^^target_type, std::meta::identifier_of(source_field));
            using target_member = typename[:std::meta::type_of(target_field):];
            using source_member = typename[:std::meta::type_of(source_field):];
            if constexpr(std::is_assignable_v<target_member&, const source_member&>)
            {
                target.[:target_field:] = source.[:source_field:];
            }
            else if constexpr((std::is_same_v<target_member, edges> || std::is_same_v<target_member, states_color>) && std::is_class_v<source_member>)
            {
                merge(target.[:target_field:], source.[:source_field:]);
            }
            else
            {
                static_assert(std::is_assignable_v<target_member&, const source_member&>, "Incompatible style property type");
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
    std::optional<std::optional<centui::font>> font = std::nullopt;
    std::optional<text_alignment> text_align = std::nullopt;
    std::optional<centui::visibility> visibility = std::nullopt;
    std::optional<centui::overflow> overflow = std::nullopt;
    std::optional<float> border_width = std::nullopt;
    std::optional<states_color> border_color = std::nullopt;

    [[nodiscard]] style apply(style candidate) const
    {
        constexpr auto context = std::meta::access_context::current();
        static constexpr auto patches = std::define_static_array(std::meta::nonstatic_data_members_of(^^style_patch, context));
        static constexpr auto properties = std::define_static_array(std::meta::nonstatic_data_members_of(^^style, context));
        template for(constexpr auto patch: patches)
        {
            template for(constexpr auto property: properties)
            {
                if constexpr(std::meta::identifier_of(patch) == std::meta::identifier_of(property))
                {
                    if(const auto& value = this->[:patch:]; value)
                    {
                        candidate.[:property:] = *value;
                    }
                }
            }
        }
        candidate.validate();
        return candidate;
    }
};
} // namespace centui



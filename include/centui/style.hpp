#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

// Properties accepted by compile-time style composition.
#define CENTUI_STYLE_PROPERTIES(X) \
    X(padding)                     \
    X(margin)                      \
    X(inset)                       \
    X(gap)                         \
    X(position)                    \
    X(width)                       \
    X(height)                      \
    X(flex_direction)              \
    X(align_items)                 \
    X(justify_content)             \
    X(background_color)            \
    X(text_color)                  \
    X(font_size)                   \
    X(font)                        \
    X(text_align)                  \
    X(visibility)                  \
    X(overflow)                    \
    X(border_width)                \
    X(border_color)

#include <centui/font.hpp>
#include <centui/types.hpp>

namespace centui
{

namespace details
{

// Check supported names explicitly; recursive overrides preserve unspecified fields.
// Edges and state colors support partial nested overrides.
template <typename...>
inline constexpr bool always_false = false;

template <typename Target, typename Source>
void merge_value(Target& target, const Source& source);

template <typename Target, typename Source>
void merge_nested(Target& target, const Source& source)
{
#define CENTUI_MERGE_NESTED(name)                                                          \
    if constexpr(requires { source.name; })                                                \
    {                                                                                      \
        static_assert(requires { target.name; }, "Unknown nested style property: " #name); \
        merge_value(target.name, source.name);                                             \
    }

    if constexpr(std::is_same_v<Target, edges>)
    {
        CENTUI_MERGE_NESTED(top)
        CENTUI_MERGE_NESTED(right)
        CENTUI_MERGE_NESTED(bottom)
        CENTUI_MERGE_NESTED(left)
    }
    else if constexpr(std::is_same_v<Target, states_color>)
    {
        CENTUI_MERGE_NESTED(normal)
        CENTUI_MERGE_NESTED(hover)
        CENTUI_MERGE_NESTED(hovered)
        CENTUI_MERGE_NESTED(pressed)
        CENTUI_MERGE_NESTED(focus)
        CENTUI_MERGE_NESTED(focused)
        CENTUI_MERGE_NESTED(disabled)
    }
#undef CENTUI_MERGE_NESTED
}

template <typename Target, typename Source>
void merge_value(Target& target, const Source& source)
{
    if constexpr(std::is_assignable_v<Target&, const Source&>)
    {
        target = source;
    }
    else if constexpr((std::is_same_v<Target, edges> || std::is_same_v<Target, states_color>) &&
                      std::is_class_v<Source>)
    {
        merge_nested(target, source);
    }
    else
    {
        static_assert(always_false<Target, Source>, "Incompatible style property type");
    }
}

enum class visual_state
{
    normal,
    hovered,
    pressed,
    focused,
    disabled
};

} // namespace details

/// \brief Tracks whether geometry or paint needs to be prepared again.
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

/// \brief Resolved appearance and layout properties for a widget.
class style
{
public:
    template <typename... types>
    static style resolve(style output)
    {
        (merge(output, types{}), ...);

        output.validate();

        return output;
    }

    template <typename... types>
    static style resolve()
    {
        return resolve<types...>(style{});
    }

    edges padding = edges::all(px(0));

    edges margin = edges::all(px(0));

    edges inset = edges::all(fit());

    length gap = px(0);

    centui::position position = centui::position::relative;

    length width = fit();

    length height = fit();

    direction flex_direction = direction::column;
    alignment align_items = alignment::stretch;
    justification justify_content = justification::start;

    states_color background_color = states_color::all({0, 0, 0, 0});

    states_color text_color = states_color::all({255, 255, 255, 255});

    float font_size = 16.0f;
    std::optional<centui::font> font = std::nullopt;
    text_alignment text_align = text_alignment::left;
    centui::visibility visibility = centui::visibility::visible;
    centui::overflow overflow = centui::overflow::visible;
    float border_width = 0;

    states_color border_color = states_color::all({0, 0, 0, 0});

    bool operator==(const style&) const = default;

    // Shared by style construction, full replacement, and patches.
    /// \brief Throws std::invalid_argument when a property cannot be represented or laid out.
    void validate() const
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

    [[nodiscard]] invalidation changes_from(const style& previous) const noexcept;

private:
    template <typename Source>
    static void merge(style& target, const Source& source)
    {
#define CENTUI_MERGE_PROPERTY(name)                     \
    if constexpr(requires { source.name; })             \
    {                                                   \
        details::merge_value(target.name, source.name); \
    }

        CENTUI_STYLE_PROPERTIES(CENTUI_MERGE_PROPERTY)
#undef CENTUI_MERGE_PROPERTY
    }
};

/// \brief Replaces only the properties supplied by the caller.
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

    [[nodiscard]] style apply(style candidate) const;
};

} // namespace centui

#undef CENTUI_STYLE_PROPERTIES

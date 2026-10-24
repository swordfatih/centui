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
    X(border_color)                \
    X(flex)                        \
    X(min_width)                   \
    X(min_height)                  \
    X(max_width)                   \
    X(max_height)                  \
    X(aspect_ratio)                \
    X(flex_wrap)                   \
    X(align_self)                  \
    X(align_content)               \
    X(row_gap)                     \
    X(column_gap)                  \
    X(layout_direction)            \
    X(box_sizing)                  \
    X(display)                     \
    X(reference_baseline)          \
    X(forms_containing_block)

#include <centui/font.hpp>
#include <centui/types.hpp>

namespace centui
{

namespace details
{

// Check supported names explicitly; recursive overrides preserve unspecified fields.
// Edges, flex sizing, and state colors support partial nested overrides.
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
        CENTUI_MERGE_NESTED(start)
        CENTUI_MERGE_NESTED(end)
        CENTUI_MERGE_NESTED(horizontal)
        CENTUI_MERGE_NESTED(vertical)
    }
    else if constexpr(std::is_same_v<Target, centui::flex>)
    {
        CENTUI_MERGE_NESTED(grow)
        CENTUI_MERGE_NESTED(shrink)
        CENTUI_MERGE_NESTED(basis)
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
    else if constexpr((std::is_same_v<Target, edges> || std::is_same_v<Target, states_color> ||
                       std::is_same_v<Target, centui::flex>) &&
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

    edges padding{};

    edges margin{};

    edges inset{};

    length gap = px(0);

    centui::position position = centui::position::relative;

    length width = automatic();

    length height = automatic();

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
    edges border_width{};

    states_color border_color = states_color::all({0, 0, 0, 0});

    centui::flex flex = {};
    length min_width = undefined();
    length min_height = undefined();
    length max_width = undefined();
    length max_height = undefined();
    std::optional<float> aspect_ratio = std::nullopt;
    wrapping flex_wrap = wrapping::none;
    alignment align_self = alignment::automatic;
    alignment align_content = alignment::start;
    length row_gap = undefined();
    length column_gap = undefined();
    centui::layout_direction layout_direction = centui::layout_direction::inherit;
    centui::box_sizing box_sizing = centui::box_sizing::border_box;
    centui::display display = centui::display::flex;
    bool reference_baseline = false;
    bool forms_containing_block = false;

    bool operator==(const style&) const = default;

    // Shared by style construction, full replacement, and patches.
    /// \brief Throws std::invalid_argument when a property cannot be represented or laid out.
    void validate() const;

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

/// \brief Replaces only supplied properties; nested values are replaced as a whole.
/// Use undefined() to clear a length; an engaged empty optional clears font or aspect ratio.
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
    std::optional<edges> border_width = std::nullopt;
    std::optional<states_color> border_color = std::nullopt;

    std::optional<centui::flex> flex = std::nullopt;
    std::optional<length> min_width = std::nullopt;
    std::optional<length> min_height = std::nullopt;
    std::optional<length> max_width = std::nullopt;
    std::optional<length> max_height = std::nullopt;
    std::optional<std::optional<float>> aspect_ratio = std::nullopt;
    std::optional<wrapping> flex_wrap = std::nullopt;
    std::optional<alignment> align_self = std::nullopt;
    std::optional<alignment> align_content = std::nullopt;
    std::optional<length> row_gap = std::nullopt;
    std::optional<length> column_gap = std::nullopt;
    std::optional<centui::layout_direction> layout_direction = std::nullopt;
    std::optional<centui::box_sizing> box_sizing = std::nullopt;
    std::optional<centui::display> display = std::nullopt;
    std::optional<bool> reference_baseline = std::nullopt;
    std::optional<bool> forms_containing_block = std::nullopt;

    [[nodiscard]] style apply(style candidate) const;
};

} // namespace centui

#undef CENTUI_STYLE_PROPERTIES

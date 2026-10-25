#include "invalidation.hpp"

#include <centui/style.hpp>

namespace centui
{

void style::validate() const
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

        case unit::undefined:
        case unit::automatic:
        {
            if(value.value != 0)
            {
                throw std::invalid_argument("Automatic and undefined lengths must have zero value");
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
        check_length(value.start, nonnegative);
        check_length(value.end, nonnegative);
        check_length(value.horizontal, nonnegative);
        check_length(value.vertical, nonnegative);
    };

    if(!std::isfinite(corner_radius) || corner_radius < 0)
    {
        throw std::invalid_argument("Corner radius must be finite and nonnegative");
    }
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

    const auto require_units = [](length value, bool allow_auto, bool allow_percent)
    {
        if((!allow_auto && value.unit == unit::automatic) || (!allow_percent && value.unit == unit::percent))
        {
            throw std::invalid_argument("Length unit is not supported by this property");
        }
    };
    const auto edge_units = [&](const edges& value, bool allow_auto, bool allow_percent)
    {
        for(auto edge:
            {value.top,
             value.right,
             value.bottom,
             value.left,
             value.start,
             value.end,
             value.horizontal,
             value.vertical})
        {
            require_units(edge, allow_auto, allow_percent);
        }
    };
    edge_units(padding, false, true);
    check_edges(border_width, true);
    edge_units(border_width, false, false);
    for(auto value: {min_width, min_height, max_width, max_height, gap, row_gap, column_gap})
    {
        check_length(value, true);
        require_units(value, false, true);
    }
    check_length(flex.basis, true);
    if(!std::isfinite(flex.grow) || flex.grow < 0 || !std::isfinite(flex.shrink) || flex.shrink < 0)
    {
        throw std::invalid_argument("Flex factors must be finite and nonnegative");
    }
    if(aspect_ratio && (!std::isfinite(*aspect_ratio) || *aspect_ratio <= 0))
    {
        throw std::invalid_argument("Aspect ratio must be finite and positive");
    }
    check_enum(flex_wrap, wrapping::none, wrapping::reverse);
    check_enum(align_self, alignment::automatic, alignment::baseline);
    check_enum(align_content, alignment::start, alignment::space_evenly);
    if(align_items == alignment::automatic || align_items > alignment::baseline || align_content == alignment::baseline)
    {
        throw std::invalid_argument("Alignment is not supported by this property");
    }
    check_enum(layout_direction, centui::layout_direction::inherit, centui::layout_direction::rtl);
    check_enum(box_sizing, centui::box_sizing::border_box, centui::box_sizing::content_box);
    check_enum(display, centui::display::flex, centui::display::contents);
}

invalidation style::changes_from(const style& previous) const noexcept
{
    const auto effects = details::classify_style(*this, previous);
    return {.layout = effects.layout, .paint = effects.paint};
}

details::style_effects details::classify_style(const style& next, const style& previous) noexcept
{
    const bool text_changed = next.font_size != previous.font_size || next.font != previous.font;
    const bool layout_changed =
        text_changed || next.padding != previous.padding || next.margin != previous.margin ||
        next.inset != previous.inset || next.gap != previous.gap || next.position != previous.position ||
        next.width != previous.width || next.height != previous.height ||
        next.flex_direction != previous.flex_direction || next.align_items != previous.align_items ||
        next.justify_content != previous.justify_content || next.visibility != previous.visibility ||
        next.overflow != previous.overflow || next.border_width != previous.border_width ||
        next.flex != previous.flex || next.min_width != previous.min_width || next.min_height != previous.min_height ||
        next.max_width != previous.max_width || next.max_height != previous.max_height ||
        next.aspect_ratio != previous.aspect_ratio || next.flex_wrap != previous.flex_wrap ||
        next.align_self != previous.align_self || next.align_content != previous.align_content ||
        next.row_gap != previous.row_gap || next.column_gap != previous.column_gap ||
        next.layout_direction != previous.layout_direction || next.box_sizing != previous.box_sizing ||
        next.display != previous.display || next.reference_baseline != previous.reference_baseline ||
        next.forms_containing_block != previous.forms_containing_block;

    return style_effects{
        .layout = layout_changed,
        .paint = layout_changed || next.background_color != previous.background_color ||
                 next.text_color != previous.text_color || next.text_align != previous.text_align ||
                 next.border_color != previous.border_color || next.corner_radius != previous.corner_radius,
        .interaction = next.visibility != previous.visibility || next.overflow != previous.overflow ||
                       next.display != previous.display,
        .inherited_metrics = text_changed,
    };
}

style style_patch::apply(style candidate) const
{
    apply_to(candidate);
    candidate.validate();
    return candidate;
}

#define CENTUI_PATCH_PROPERTIES(X) \
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
    X(forms_containing_block)      \
    X(corner_radius)

void style_patch::apply_to(style& candidate) const
{
#define CENTUI_APPLY_PROPERTY(name) \
    if(name)                        \
    {                               \
        candidate.name = *name;     \
    }
    CENTUI_PATCH_PROPERTIES(CENTUI_APPLY_PROPERTY)
#undef CENTUI_APPLY_PROPERTY
}

void style_patch::merge(const style_patch& patch)
{
#define CENTUI_MERGE_PROPERTY(name) \
    if(patch.name)                  \
    {                               \
        name = patch.name;          \
    }
    CENTUI_PATCH_PROPERTIES(CENTUI_MERGE_PROPERTY)
#undef CENTUI_MERGE_PROPERTY
}

style_patch style_patch::replacement(const style& value)
{
    style_patch result;
#define CENTUI_REPLACE_PROPERTY(name) result.name.emplace(value.name);
    CENTUI_PATCH_PROPERTIES(CENTUI_REPLACE_PROPERTY)
#undef CENTUI_REPLACE_PROPERTY
    return result;
}

#undef CENTUI_PATCH_PROPERTIES

} // namespace centui

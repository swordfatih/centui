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
    const bool text_changed = font_size != previous.font_size || font != previous.font;
    const bool layout_changed =
        text_changed || padding != previous.padding || margin != previous.margin || inset != previous.inset ||
        gap != previous.gap || position != previous.position || width != previous.width || height != previous.height ||
        flex_direction != previous.flex_direction || align_items != previous.align_items ||
        justify_content != previous.justify_content || visibility != previous.visibility ||
        overflow != previous.overflow || border_width != previous.border_width || flex != previous.flex ||
        min_width != previous.min_width || min_height != previous.min_height || max_width != previous.max_width ||
        max_height != previous.max_height || aspect_ratio != previous.aspect_ratio || flex_wrap != previous.flex_wrap ||
        align_self != previous.align_self || align_content != previous.align_content || row_gap != previous.row_gap ||
        column_gap != previous.column_gap || layout_direction != previous.layout_direction ||
        box_sizing != previous.box_sizing || display != previous.display ||
        reference_baseline != previous.reference_baseline || forms_containing_block != previous.forms_containing_block;

    return {
        .layout = layout_changed,
        .paint = layout_changed || background_color != previous.background_color || text_color != previous.text_color ||
                 text_align != previous.text_align || border_color != previous.border_color ||
                 corner_radius != previous.corner_radius,
    };
}

style style_patch::apply(style candidate) const
{
#define CENTUI_APPLY_PATCH(name) \
    if(name)                     \
    {                            \
        candidate.name = *name;  \
    }

    CENTUI_APPLY_PATCH(padding)
    CENTUI_APPLY_PATCH(margin)
    CENTUI_APPLY_PATCH(inset)
    CENTUI_APPLY_PATCH(gap)
    CENTUI_APPLY_PATCH(position)
    CENTUI_APPLY_PATCH(width)
    CENTUI_APPLY_PATCH(height)
    CENTUI_APPLY_PATCH(flex_direction)
    CENTUI_APPLY_PATCH(align_items)
    CENTUI_APPLY_PATCH(justify_content)
    CENTUI_APPLY_PATCH(background_color)
    CENTUI_APPLY_PATCH(text_color)
    CENTUI_APPLY_PATCH(font_size)
    CENTUI_APPLY_PATCH(font)
    CENTUI_APPLY_PATCH(text_align)
    CENTUI_APPLY_PATCH(visibility)
    CENTUI_APPLY_PATCH(overflow)
    CENTUI_APPLY_PATCH(border_width)
    CENTUI_APPLY_PATCH(border_color)
    CENTUI_APPLY_PATCH(flex)
    CENTUI_APPLY_PATCH(min_width)
    CENTUI_APPLY_PATCH(min_height)
    CENTUI_APPLY_PATCH(max_width)
    CENTUI_APPLY_PATCH(max_height)
    CENTUI_APPLY_PATCH(aspect_ratio)
    CENTUI_APPLY_PATCH(flex_wrap)
    CENTUI_APPLY_PATCH(align_self)
    CENTUI_APPLY_PATCH(align_content)
    CENTUI_APPLY_PATCH(row_gap)
    CENTUI_APPLY_PATCH(column_gap)
    CENTUI_APPLY_PATCH(layout_direction)
    CENTUI_APPLY_PATCH(box_sizing)
    CENTUI_APPLY_PATCH(display)
    CENTUI_APPLY_PATCH(reference_baseline)
    CENTUI_APPLY_PATCH(forms_containing_block)
    CENTUI_APPLY_PATCH(corner_radius)
#undef CENTUI_APPLY_PATCH
    candidate.validate();

    return candidate;
}

} // namespace centui

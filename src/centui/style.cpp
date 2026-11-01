#include "invalidation.hpp"

#include <centui/style.hpp>

namespace centui
{

void style::validate() const
{
    if(paragraph)
    {
        paragraph->validate();
    }
    if(font)
    {
        font->validate();
    }
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
    if(!std::isfinite(focus.width) || focus.width < 0 || !std::isfinite(focus.inset) || focus.inset < 0)
    {
        throw std::invalid_argument("Focus indicator dimensions must be finite and nonnegative");
    }
    check_length(width, true);
    check_length(height, true);
    check_length(gap, true);
    check_edges(padding, true);
    check_edges(margin, false);
    check_edges(inset, false);

    if(!std::isfinite(font_size) || font_size <= 0 ||
       static_cast<double>(font_size) * 64 > std::numeric_limits<std::int32_t>::max())
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
    check_enum(text_align, centypo::text_alignment::left, centypo::text_alignment::end);
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
    style_effects effects;
    effects.paint = next != previous;
    effects.inherited_metrics =
        next.font_size != previous.font_size || next.font != previous.font || next.paragraph != previous.paragraph;
    effects.interaction = next.visibility != previous.visibility || next.overflow != previous.overflow ||
                          next.display != previous.display;
    effects.layout = effects.inherited_metrics || effects.interaction || next.padding != previous.padding ||
                     next.margin != previous.margin || next.inset != previous.inset || next.gap != previous.gap ||
                     next.position != previous.position || next.width != previous.width ||
                     next.height != previous.height || next.flex_direction != previous.flex_direction ||
                     next.align_items != previous.align_items || next.justify_content != previous.justify_content ||
                     next.border_width != previous.border_width || next.flex != previous.flex ||
                     next.min_width != previous.min_width || next.min_height != previous.min_height ||
                     next.max_width != previous.max_width || next.max_height != previous.max_height ||
                     next.aspect_ratio != previous.aspect_ratio || next.flex_wrap != previous.flex_wrap ||
                     next.align_self != previous.align_self || next.align_content != previous.align_content ||
                     next.row_gap != previous.row_gap || next.column_gap != previous.column_gap ||
                     next.layout_direction != previous.layout_direction || next.box_sizing != previous.box_sizing ||
                     next.reference_baseline != previous.reference_baseline ||
                     next.forms_containing_block != previous.forms_containing_block;
    return effects;
}

style style_patch::apply(style candidate) const
{
    apply_to(candidate);
    candidate.validate();
    return candidate;
}

void style_patch::apply_to(style& candidate) const
{
    const auto apply = [](auto& target, const auto& value)
    {
        if(value)
        {
            target = *value;
        }
    };
    apply(candidate.padding, padding);
    apply(candidate.margin, margin);
    apply(candidate.inset, inset);
    apply(candidate.gap, gap);
    apply(candidate.position, position);
    apply(candidate.width, width);
    apply(candidate.height, height);
    apply(candidate.flex_direction, flex_direction);
    apply(candidate.align_items, align_items);
    apply(candidate.justify_content, justify_content);
    apply(candidate.background_color, background_color);
    apply(candidate.text_color, text_color);
    apply(candidate.font_size, font_size);
    apply(candidate.font, font);
    apply(candidate.text_align, text_align);
    apply(candidate.paragraph, paragraph);
    apply(candidate.visibility, visibility);
    apply(candidate.overflow, overflow);
    apply(candidate.border_width, border_width);
    apply(candidate.border_color, border_color);
    apply(candidate.flex, flex);
    apply(candidate.min_width, min_width);
    apply(candidate.min_height, min_height);
    apply(candidate.max_width, max_width);
    apply(candidate.max_height, max_height);
    apply(candidate.aspect_ratio, aspect_ratio);
    apply(candidate.flex_wrap, flex_wrap);
    apply(candidate.align_self, align_self);
    apply(candidate.align_content, align_content);
    apply(candidate.row_gap, row_gap);
    apply(candidate.column_gap, column_gap);
    apply(candidate.layout_direction, layout_direction);
    apply(candidate.box_sizing, box_sizing);
    apply(candidate.display, display);
    apply(candidate.reference_baseline, reference_baseline);
    apply(candidate.forms_containing_block, forms_containing_block);
    apply(candidate.corner_radius, corner_radius);
    apply(candidate.focus, focus);
}

void style_patch::merge(const style_patch& patch)
{
    const auto merge = [](auto& target, const auto& value)
    {
        if(value)
        {
            target = value;
        }
    };
    merge(padding, patch.padding);
    merge(margin, patch.margin);
    merge(inset, patch.inset);
    merge(gap, patch.gap);
    merge(position, patch.position);
    merge(width, patch.width);
    merge(height, patch.height);
    merge(flex_direction, patch.flex_direction);
    merge(align_items, patch.align_items);
    merge(justify_content, patch.justify_content);
    merge(background_color, patch.background_color);
    merge(text_color, patch.text_color);
    merge(font_size, patch.font_size);
    merge(font, patch.font);
    merge(text_align, patch.text_align);
    merge(paragraph, patch.paragraph);
    merge(visibility, patch.visibility);
    merge(overflow, patch.overflow);
    merge(border_width, patch.border_width);
    merge(border_color, patch.border_color);
    merge(flex, patch.flex);
    merge(min_width, patch.min_width);
    merge(min_height, patch.min_height);
    merge(max_width, patch.max_width);
    merge(max_height, patch.max_height);
    merge(aspect_ratio, patch.aspect_ratio);
    merge(flex_wrap, patch.flex_wrap);
    merge(align_self, patch.align_self);
    merge(align_content, patch.align_content);
    merge(row_gap, patch.row_gap);
    merge(column_gap, patch.column_gap);
    merge(layout_direction, patch.layout_direction);
    merge(box_sizing, patch.box_sizing);
    merge(display, patch.display);
    merge(reference_baseline, patch.reference_baseline);
    merge(forms_containing_block, patch.forms_containing_block);
    merge(corner_radius, patch.corner_radius);
    merge(focus, patch.focus);
}

style_patch style_patch::replacement(const style& value)
{
    style_patch result;
    result.padding.emplace(value.padding);
    result.margin.emplace(value.margin);
    result.inset.emplace(value.inset);
    result.gap.emplace(value.gap);
    result.position.emplace(value.position);
    result.width.emplace(value.width);
    result.height.emplace(value.height);
    result.flex_direction.emplace(value.flex_direction);
    result.align_items.emplace(value.align_items);
    result.justify_content.emplace(value.justify_content);
    result.background_color.emplace(value.background_color);
    result.text_color.emplace(value.text_color);
    result.font_size.emplace(value.font_size);
    result.font.emplace(value.font);
    result.text_align.emplace(value.text_align);
    result.paragraph.emplace(value.paragraph);
    result.visibility.emplace(value.visibility);
    result.overflow.emplace(value.overflow);
    result.border_width.emplace(value.border_width);
    result.border_color.emplace(value.border_color);
    result.flex.emplace(value.flex);
    result.min_width.emplace(value.min_width);
    result.min_height.emplace(value.min_height);
    result.max_width.emplace(value.max_width);
    result.max_height.emplace(value.max_height);
    result.aspect_ratio.emplace(value.aspect_ratio);
    result.flex_wrap.emplace(value.flex_wrap);
    result.align_self.emplace(value.align_self);
    result.align_content.emplace(value.align_content);
    result.row_gap.emplace(value.row_gap);
    result.column_gap.emplace(value.column_gap);
    result.layout_direction.emplace(value.layout_direction);
    result.box_sizing.emplace(value.box_sizing);
    result.display.emplace(value.display);
    result.reference_baseline.emplace(value.reference_baseline);
    result.forms_containing_block.emplace(value.forms_containing_block);
    result.corner_radius.emplace(value.corner_radius);
    result.focus.emplace(value.focus);
    return result;
}

} // namespace centui

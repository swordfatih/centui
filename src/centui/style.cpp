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
#define CENTUI_STYLE_PROPERTY(name, affects_layout, affects_interaction, inherited) \
    if(next.name != previous.name)                                                  \
    {                                                                               \
        effects.paint = true;                                                       \
        effects.layout |= affects_layout;                                           \
        effects.interaction |= affects_interaction;                                 \
        effects.inherited_metrics |= inherited;                                     \
    }
#include <centui/details/style_properties.inc>
#undef CENTUI_STYLE_PROPERTY
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
#define CENTUI_STYLE_PROPERTY(name, ...) \
    if(name)                             \
    {                                    \
        candidate.name = *name;          \
    }
#include <centui/details/style_properties.inc>
#undef CENTUI_STYLE_PROPERTY
}

void style_patch::merge(const style_patch& patch)
{
#define CENTUI_STYLE_PROPERTY(name, ...) \
    if(patch.name)                       \
    {                                    \
        name = patch.name;               \
    }
#include <centui/details/style_properties.inc>
#undef CENTUI_STYLE_PROPERTY
}

style_patch style_patch::replacement(const style& value)
{
    style_patch result;
#define CENTUI_STYLE_PROPERTY(name, ...) result.name.emplace(value.name);
#include <centui/details/style_properties.inc>
#undef CENTUI_STYLE_PROPERTY
    return result;
}

} // namespace centui

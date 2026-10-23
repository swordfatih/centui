#include <centui/style.hpp>

namespace centui
{

invalidation style::changes_from(const style& previous) const noexcept
{
    const bool text_changed = font_size != previous.font_size || font != previous.font;
    const bool layout_changed = text_changed || padding != previous.padding || margin != previous.margin ||
                                inset != previous.inset || gap != previous.gap || position != previous.position ||
                                width != previous.width || height != previous.height ||
                                flex_direction != previous.flex_direction || align_items != previous.align_items ||
                                justify_content != previous.justify_content || visibility != previous.visibility ||
                                overflow != previous.overflow || border_width != previous.border_width;

    return {
        .layout = layout_changed,
        .paint = layout_changed || background_color != previous.background_color || text_color != previous.text_color ||
                 text_align != previous.text_align || border_color != previous.border_color,
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
#undef CENTUI_APPLY_PATCH
    candidate.validate();

    return candidate;
}

} // namespace centui

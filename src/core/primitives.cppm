module;

#include <cstddef>
#include <memory>
#include <string_view>
#include <variant>
#include <vector>

export module centui.primitives;

import centui.model;
import centui.style;
import centui.unit;
import centui.glyph;
import centui.text_service;
import centui.layout_engine;
import centui.diagnostics;
import centui.draw;

export namespace centui::details
{

inline bool primitive_has_children(const element_content& content)
{
    return std::holds_alternative<container_data>(content.data);
}

inline centui::style resolve_primitive_style(const element_content& content, const centui::style* parent)
{
    auto result = content.style;
    if(content.inherit_text_style && parent)
    {
        result.font_size = parent->font_size;
        result.text_color = parent->text_color;
        result.text_align = parent->text_align;
    }
    return result;
}

// The coordinator schedules preparation; primitives supply intrinsic geometry.
inline bool prepare_primitive(std::size_t index, const element_content& content, const centui::style& style,
                              text_service& text, layout_engine& layout, font& face,
                              const std::shared_ptr<const void>& identity, preparation_diagnostics& diagnostics)
{
    if(const auto* label = std::get_if<text_content>(&content.data))
    {
        const auto& prepared = text.prepare(index, face, identity, label->value, style.font_size, diagnostics);
        return layout.measure(index, prepared->bounds, diagnostics);
    }
    return false;
}

inline void paint_primitive(const element_content& content, const centui::style& style, rect bounds,
                            const text_layout* shaped, visual_state visual, std::vector<draw_command>& output)
{
    std::string_view value;
    if(const auto* label = std::get_if<text_content>(&content.data))
    {
        value = label->value;
    }
    append_paint(style, bounds, value, shaped, visual, output);
}

} // namespace centui::details

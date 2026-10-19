module;

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

export module centui.preparation;

import centui.runtime;
import centui.model;
import centui.style;
import centui.unit;
import centui.glyph;
import centui.draw;
import centui.painter;
import centui.diagnostics;

export namespace centui::details
{

inline std::optional<std::string_view> text_value(const element_content& content)
{
    if(const auto* text = std::get_if<text_content>(&content.data))
    {
        return text->value;
    }
    if(const auto* button = std::get_if<button_content>(&content.data))
    {
        return button->value;
    }
    return std::nullopt;
}

// Coordinates stage dependencies. Services own their caches and native state.
inline void prepare_geometry(document_state& state, font& face, int width, int height)
{
    const auto root = state.root();
    if(!root)
    {
        return;
    }

    const auto identity = face.identity();
    auto&      previous = state.geometry;
    const bool viewport_changed = !previous.valid || previous.width != width || previous.height != height;
    const bool font_changed = previous.face != identity;
    const auto dirty = state.pending();
    if(!viewport_changed && !font_changed && !dirty.topology && !dirty.text && !dirty.layout)
    {
        return;
    }

    // Failed preparation cannot publish geometry or paint as current.
    previous.valid = false;
    state.paint.invalidate();
    const auto order = state.preorder(*root);
    bool       calculate = viewport_changed;

    // Release every changed owner before attaching any new children.
    for(const auto id: order)
    {
        if(state.get(id).pending.topology)
        {
            state.layout.detach_children(id.index);
        }
    }

    for(const auto id: order)
    {
        const auto& content = state.get(id).content;
        calculate |= state.layout.apply_style(id.index, content.style, state.diagnostics);
        if(const auto value = text_value(content))
        {
            const auto& prepared = state.text.prepare(id.index, face, identity, *value,
                                                      content.style.font_size, state.diagnostics);
            calculate |= state.layout.measure(id.index, prepared, state.diagnostics);
        }
    }

    for(const auto id: order)
    {
        const auto& record = state.get(id);
        if(std::holds_alternative<container_data>(record.content.data))
        {
            calculate |= state.layout.synchronize_children(id.index, record.children, state.diagnostics);
        }
    }
    if(calculate)
    {
        state.layout.calculate(root->index, width, height, state.diagnostics);
    }

    previous.face = identity;
    previous.width = width;
    previous.height = height;
    previous.valid = true;
    state.complete(*root, {.topology = true, .text = true, .layout = true});
}

inline std::span<const draw_command> prepared_commands(document_state& state, node_id selected,
                                                       font& face, int width, int height)
{
    if(width <= 0 || height <= 0)
    {
        return {};
    }
    (void)state.get(selected);
    prepare_geometry(state, face, width, height);
    if(!state.paint.valid())
    {
        const auto              root = *state.root();
        const auto              order = state.preorder(root);
        std::vector<rect>       bounds(state.slot_count());
        std::vector<paint_item> items;
        items.reserve(order.size());
        for(const auto id: order)
        {
            const auto&                record = state.get(id);
            auto                       absolute = state.layout.bounds(id.index);
            std::optional<std::size_t> parent;
            if(record.parent)
            {
                parent = record.parent->index;
                absolute.x += bounds[*parent].x;
                absolute.y += bounds[*parent].y;
            }
            bounds[id.index] = absolute;
            items.push_back({id.index, parent, absolute, &record.content, state.text.get(id.index)});
        }

        state.paint.rebuild(items, state.slot_count(), state.diagnostics);
        state.complete(root, {.paint = true});
    }
    return state.paint.commands(selected.index);
}

} // namespace centui::details

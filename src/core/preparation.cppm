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
import centui.primitives;

export namespace centui::details
{

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
    const auto                 order = state.preorder(*root);
    bool                       calculate = viewport_changed;
    std::vector<centui::style> styles(state.slot_count());

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
        const auto& record = state.get(id);
        const auto& content = record.content;
        const auto* parent = record.parent ? &styles[record.parent->index] : nullptr;
        auto&       style = styles[id.index];
        style = resolve_primitive_style(content, parent);
        calculate |= state.layout.apply_style(id.index, style, state.diagnostics);
        calculate |= prepare_primitive(id.index, content, style, state.text, state.layout,
                                       face, identity, state.diagnostics);
    }

    for(const auto id: order)
    {
        const auto& record = state.get(id);
        if(primitive_has_children(record.content))
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
        const auto                 root = *state.root();
        const auto                 order = state.preorder(root);
        std::vector<rect>          bounds(state.slot_count());
        std::vector<paint_item>    items;
        std::vector<centui::style> styles(state.slot_count());
        std::vector<visual_state>  visuals(state.slot_count(), visual_state::normal);
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
            styles[id.index] = resolve_primitive_style(record.content, parent ? &styles[*parent] : nullptr);
            visuals[id.index] = state.appearance(id, parent ? visuals[*parent] : visual_state::normal);
            items.push_back({id.index, parent, absolute, &record.content, state.text.get(id.index),
                             styles[id.index], visuals[id.index]});
        }

        state.paint.rebuild(items, state.slot_count(), state.diagnostics);
        state.complete(root, {.paint = true});
    }
    return state.paint.commands(selected.index);
}

} // namespace centui::details

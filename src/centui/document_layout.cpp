#include "document_state.hpp"

namespace centui
{

void document::set_layout_options(layout_options options)
{
    m_data->context->check_mutation();
    options.validate();

    if(m_data->geometry.layout_options != options)
    {
        m_data->geometry.viewport_layout.reset();
        m_data->geometry.tree.reset();
        m_data->geometry.configuration.reset();
        m_data->geometry.layout_options = options;
        m_data->geometry.measure_all = true;
        dirty_tree();
    }
}

const layout_options& document::options() const noexcept
{
    return m_data->geometry.layout_options;
}

void document::sync_layout(
    std::unique_ptr<layout_entry>& entry,
    const std::shared_ptr<widget>& value,
    bool inherited_dirty
)
{
    bool created = false;
    if(!entry || entry->value != value)
    {
        entry = std::make_unique<layout_entry>(*m_data->geometry.configuration);
        ++m_data->statistics.layout_nodes_created;
        entry->value = value;
        created = true;
    }

    if(!created && !inherited_dirty && !value->m_layout_dirty && !value->m_layout_descendants_dirty)
    {
        return;
    }

    const auto& appearance = value->m_style;
    const auto effects = entry->applied_style ? details::classify_style(appearance, *entry->applied_style)
                                              : details::style_effects{true, true, true, true};
    const bool text_changed = inherited_dirty || effects.inherited_metrics;
    const bool dirty = value->m_layout_dirty || text_changed;
    if(created || dirty || effects.layout)
    {
        ++m_data->statistics.layout_nodes_synchronized;
    }
    if(effects.layout)
    {
        entry->native.update(appearance);
    }
    entry->applied_style = appearance;

    const bool measured = value->m_children.empty() && appearance.display != display::contents;
    if(measured != entry->measured)
    {
        entry->native.remove_children();
        if(measured)
        {
            entry->native.set_measure(
                [this, value](axis_constraint width, axis_constraint height)
                {
                    ++m_data->statistics.measurement_calls;
                    return value->measure({width, height, m_data->fonts});
                }
            );
        }
        else
        {
            entry->native.set_measure({});
        }
        entry->measured = measured;
    }

    const bool topology_changed =
        entry->children.size() != value->m_children.size() || !std::equal(
                                                                  entry->children.begin(),
                                                                  entry->children.end(),
                                                                  value->m_children.begin(),
                                                                  [](const auto& child, const auto& desired)
                                                                  {
                                                                      return child->value == desired;
                                                                  }
                                                              );
    if(topology_changed)
    {
        // Detach native links before moving or destroying their owning entries.
        entry->native.remove_children();
        auto previous = std::move(entry->children);
        entry->children.clear();
        entry->children.reserve(value->m_children.size());
        for(const auto& child: value->m_children)
        {
            auto found = std::find_if(
                previous.begin(),
                previous.end(),
                [&](const auto& candidate)
                {
                    return candidate && candidate->value == child;
                }
            );
            std::unique_ptr<layout_entry> next;
            if(found != previous.end())
            {
                next = std::move(*found);
            }
            sync_layout(next, child, text_changed);
            entry->native.insert_child(next->native);
            entry->children.push_back(std::move(next));
        }
    }
    else
    {
        for(auto& child: entry->children)
        {
            sync_layout(child, child->value, text_changed);
        }
    }

    if(dirty)
    {
        entry->native.invalidate_measure();
        entry->native.set_baseline(value->baseline());
    }
    value->m_layout_dirty = false;
    value->m_layout_descendants_dirty = false;
}

void document::check_measurements(layout_entry& entry)
{
    entry.native.rethrow_measure_failure();

    for(auto& child: entry.children)
    {
        check_measurements(*child);
    }
}

void document::store_bounds(layout_entry& entry, float x, float y, bool force)
{
    if(!force && !entry.native.has_new_layout())
    {
        return;
    }
    entry.native.mark_layout_seen();
    if(entry.value->m_style.display == display::contents)
    {
        // Yoga positions descendants against the nearest ancestor that generates a box.
        entry.value->m_bounds = {};
        entry.value->m_content = {};
        entry.value->m_metrics = {};

        for(auto& child: entry.children)
        {
            store_bounds(*child, x, y, true);
        }
        return;
    }

    auto box = entry.native.bounds();
    box.x += x;
    box.y += y;
    const bool moved = entry.value->m_bounds.x != box.x || entry.value->m_bounds.y != box.y;
    entry.value->m_bounds = box;
    auto content = entry.native.content_bounds();
    content.x += box.x;
    content.y += box.y;
    entry.value->m_content = content;
    entry.value->m_metrics = entry.native.metrics();

    for(auto& child: entry.children)
    {
        store_bounds(
            *child,
            box.x + entry.value->m_child_offset.x,
            box.y + entry.value->m_child_offset.y,
            force || moved
        );
    }
}

void document::update_layout(centui::viewport viewport)
{
    if(!std::isfinite(viewport.scale) || viewport.scale <= 0 || !std::isfinite(viewport.width) ||
       !std::isfinite(viewport.height) || viewport.width < 0 || viewport.height < 0)
    {
        throw std::invalid_argument("Viewport dimensions must be finite and nonnegative; scale must be positive");
    }

    m_data->fonts.set_scale(viewport.scale);
    if(m_data->font_revision != m_data->fonts.identity())
    {
        m_data->geometry.measure_all = true;
        dirty_tree();
    }
    if(viewport != m_data->viewport)
    {
        const bool logical_size_changed = viewport.logical_width() != m_data->viewport.logical_width() ||
                                          viewport.logical_height() != m_data->viewport.logical_height();
        if(viewport.scale != m_data->viewport.scale)
        {
            m_data->input.pointer_x *= m_data->viewport.scale / viewport.scale;
            m_data->input.pointer_y *= m_data->viewport.scale / viewport.scale;
            m_data->input.text_input_area.reset();
            details::invalidation_access::invalidate_document(*m_data->context, details::change::raster);
        }
        m_data->viewport = viewport;
        m_data->input.hit_dirty = true;
        if(logical_size_changed)
        {
            details::invalidation_access::invalidate_document(*m_data->context, details::change::layout);
        }
    }
    auto& context = *m_data->context;
    if((!context.layout_dirty && !context.placement_dirty) || viewport.width <= 0 || viewport.height <= 0)
    {
        return;
    }

    try
    {
        phase_guard guard{context};
        if(m_data->root.valid())
        {
            if(context.layout_dirty)
            {
                if(!m_data->geometry.configuration)
                {
                    m_data->geometry.configuration =
                        std::make_unique<layout_configuration>(m_data->geometry.layout_options);
                }
                sync_layout(m_data->geometry.tree, m_data->root.m_value, m_data->geometry.measure_all);
                if(m_data->root.style().display == display::contents)
                {
                    if(!m_data->geometry.viewport_layout)
                    {
                        m_data->geometry.viewport_layout = std::make_unique<layout>(*m_data->geometry.configuration);
                        m_data->geometry.viewport_layout->insert_child(m_data->geometry.tree->native);
                    }
                    centui::style appearance;
                    appearance.width = px(viewport.logical_width());
                    appearance.height = px(viewport.logical_height());
                    m_data->geometry.viewport_layout->update(appearance);
                    ++m_data->statistics.layout_calculations;
                    m_data->geometry.viewport_layout->calculate(viewport.logical_width(), viewport.logical_height());
                }
                else
                {
                    m_data->geometry.viewport_layout.reset();
                    ++m_data->statistics.layout_calculations;
                    m_data->geometry.tree->native.calculate(viewport.logical_width(), viewport.logical_height());
                }
                check_measurements(*m_data->geometry.tree);
            }
            store_bounds(*m_data->geometry.tree, 0, 0, context.placement_dirty);
        }
        else
        {
            m_data->geometry.viewport_layout.reset();
            m_data->geometry.tree.reset();
        }
    }
    catch(...)
    {
        // Synchronization and calculation acknowledgements are independent.
        // Retry every measurement/baseline, including failures cached by Yoga.
        m_data->geometry.measure_all = true;
        context.layout_dirty = true;
        throw;
    }
    context.layout_dirty = false;
    context.placement_dirty = false;
    m_data->geometry.measure_all = false;
    details::invalidation_access::geometry_applied(context);
    m_data->font_revision = m_data->fonts.identity();
    const auto schedule = [&](auto&& self, const std::shared_ptr<widget>& value) -> void
    {
        if(!value)
        {
            return;
        }
        value->m_geometry_dirty = true;
        for(const auto& child: value->m_children)
        {
            self(self, child);
        }
    };
    schedule(schedule, m_data->root.m_value);
    context.geometry_dirty = true;
}

void document::refresh_geometry(const std::shared_ptr<widget>& value)
{
    auto& context = *m_data->context;
    if(!value || value->m_document.lock() != m_data->context || context.layout_dirty || context.placement_dirty)
    {
        return;
    }

    if(value->m_geometry_dirty)
    {
        value->m_geometry_dirty = false;
        ++m_data->statistics.geometry_callbacks;
        try
        {
            value->on_geometry();
        }
        catch(...)
        {
            value->m_geometry_dirty = true;
            context.geometry_dirty = true;
            throw;
        }
    }

    for(const auto& child: value->m_children)
    {
        refresh_geometry(child);
    }
}

} // namespace centui

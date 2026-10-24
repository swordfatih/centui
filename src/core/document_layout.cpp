#include "document_state.hpp"

namespace centui
{

void document::set_layout_options(layout_options options)
{
    m_data->context->check_mutation();
    options.validate();

    if(m_data->layout_options != options)
    {
        m_data->viewport_layout.reset();
        m_data->tree.reset();
        m_data->configuration.reset();
        m_data->layout_options = options;
        m_data->measure_all = true;
        dirty_tree();
    }
}

const layout_options& document::options() const noexcept
{
    return m_data->layout_options;
}

void document::sync_layout(
    std::unique_ptr<layout_entry>& entry,
    const std::shared_ptr<widget>& value,
    bool inherited_dirty
)
{
    if(!entry || entry->value != value)
    {
        entry = std::make_unique<layout_entry>(*m_data->configuration);
        entry->value = value;
    }

    const auto& appearance = value->m_style;
    const bool text_changed = inherited_dirty || !entry->applied_style ||
                              entry->applied_style->font != appearance.font ||
                              entry->applied_style->font_size != appearance.font_size;
    const bool dirty = value->m_measure_dirty || text_changed;
    if(!entry->applied_style || *entry->applied_style != appearance)
    {
        entry->native.update(appearance);
        entry->applied_style = appearance;
    }

    const bool measured = value->m_children.empty() && appearance.display != display::contents;
    if(measured != entry->measured)
    {
        entry->native.remove_children();
        if(measured)
        {
            entry->native.set_measure(
                [this, value](axis_constraint width, axis_constraint height)
                {
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
    value->m_measure_dirty = false;
}

void document::check_measurements(layout_entry& entry)
{
    entry.native.rethrow_measure_failure();

    for(auto& child: entry.children)
    {
        check_measurements(*child);
    }
}

void document::store_bounds(layout_entry& entry, float x, float y)
{
    if(entry.value->m_style.display == display::contents)
    {
        // Yoga positions descendants against the nearest ancestor that generates a box.
        entry.value->m_bounds = {};
        entry.value->m_content = {};
        entry.value->m_metrics = {};

        for(auto& child: entry.children)
        {
            store_bounds(*child, x, y);
        }
        return;
    }

    auto box = entry.native.bounds();
    box.x += x;
    box.y += y;
    entry.value->m_bounds = box;
    auto content = entry.native.content_bounds();
    content.x += box.x;
    content.y += box.y;
    entry.value->m_content = content;
    entry.value->m_metrics = entry.native.metrics();

    for(auto& child: entry.children)
    {
        store_bounds(*child, box.x + entry.value->m_child_offset.x, box.y + entry.value->m_child_offset.y);
    }
}

void document::update_layout(centui::viewport viewport)
{
    if(!std::isfinite(viewport.scale) || viewport.scale <= 0)
    {
        throw std::invalid_argument("Viewport scale must be finite and positive");
    }

    m_data->fonts.set_scale(viewport.scale);
    if(m_data->font_revision != m_data->fonts.identity())
    {
        m_data->measure_all = true;
        dirty_tree();
    }
    if(viewport != m_data->viewport)
    {
        if(viewport.scale != m_data->viewport.scale)
        {
            m_data->pointer_x *= m_data->viewport.scale / viewport.scale;
            m_data->pointer_y *= m_data->viewport.scale / viewport.scale;
            m_data->text_input_area.reset();
        }
        m_data->viewport = viewport;
        dirty_tree();
    }
    auto& context = *m_data->context;
    if((!context.layout_dirty && !context.placement_dirty) || viewport.width <= 0 || viewport.height <= 0)
    {
        return;
    }

    {
        phase_guard guard{context};
        if(m_data->root.valid())
        {
            if(context.layout_dirty)
            {
                if(!m_data->configuration)
                {
                    m_data->configuration = std::make_unique<layout_configuration>(m_data->layout_options);
                }
                sync_layout(m_data->tree, m_data->root.m_value, m_data->measure_all);
                if(m_data->root.style().display == display::contents)
                {
                    if(!m_data->viewport_layout)
                    {
                        m_data->viewport_layout = std::make_unique<layout>(*m_data->configuration);
                        m_data->viewport_layout->insert_child(m_data->tree->native);
                    }
                    centui::style appearance;
                    appearance.width = px(viewport.logical_width());
                    appearance.height = px(viewport.logical_height());
                    m_data->viewport_layout->update(appearance);
                    m_data->viewport_layout->calculate(viewport.logical_width(), viewport.logical_height());
                }
                else
                {
                    m_data->viewport_layout.reset();
                    m_data->tree->native.calculate(viewport.logical_width(), viewport.logical_height());
                }
                check_measurements(*m_data->tree);
            }
            store_bounds(*m_data->tree, 0, 0);
        }
        else
        {
            m_data->viewport_layout.reset();
            m_data->tree.reset();
        }
    }
    context.layout_dirty = false;
    context.placement_dirty = false;
    context.paint_dirty = true;
    m_data->measure_all = false;
    m_data->hit_dirty = true;
    m_data->font_revision = m_data->fonts.identity();
    try
    {
        notify_layout(m_data->root.m_value);
    }
    catch(...)
    {
        // A hook may have failed before rebuilding its retained geometry.
        // Retry notifications on the next preparation even if Yoga is clean.
        context.placement_dirty = true;
        throw;
    }
}

void document::notify_layout(const std::shared_ptr<widget>& value)
{
    if(!value)
    {
        return;
    }

    value->on_layout();

    for(const auto& child: value->m_children)
    {
        notify_layout(child);
    }
}

} // namespace centui

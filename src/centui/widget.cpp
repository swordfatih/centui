#include "document_state.hpp"

namespace centui
{

const centui::style& widget::style() const noexcept
{
    return m_style;
}

centui::style widget::resolved_style() const
{
    auto result = m_style;

    for(auto ancestor = m_parent.lock(); !result.font && ancestor; ancestor = ancestor->m_parent.lock())
    {
        result.font = ancestor->m_style.font;
    }

    return result;
}

rect widget::content_bounds() const noexcept
{
    return m_content;
}

bool widget::hovered() const noexcept
{
    return m_hovered;
}

bool widget::pressed() const noexcept
{
    return m_pressed;
}

bool widget::focused() const noexcept
{
    return m_focused;
}

bool widget::effectively_enabled() const noexcept
{
    if(!m_enabled)
    {
        return false;
    }

    for(auto parent = m_parent.lock(); parent; parent = parent->m_parent.lock())
    {
        if(!parent->m_enabled)
        {
            return false;
        }
    }

    return true;
}

bool widget::effectively_visible() const noexcept
{
    if(m_style.visibility != visibility::visible || m_style.display == display::none)
    {
        return false;
    }

    for(auto ancestor = m_parent.lock(); ancestor; ancestor = ancestor->m_parent.lock())
    {
        if(ancestor->m_style.visibility != visibility::visible || ancestor->m_style.display == display::none)
        {
            return false;
        }
    }

    return true;
}

centui::style widget::resolve_style(
    const style_patch& defaults,
    const details::style_patch_map& patches,
    details::style_patch_reference local
) const
{
    centui::style result;
    defaults.apply_to(result);
    for(const auto& layer: m_style_recipe.get().composition)
    {
        layer.apply(result);
        if(auto patch = patches.find(layer.id); patch != patches.end())
        {
            patch->second.apply_to(result);
        }
    }
    if(local)
    {
        local->get().apply_to(result);
    }
    result.validate();
    return result;
}

details::style_patch_reference widget::local_style() const noexcept
{
    if(m_style_overrides)
    {
        return std::cref(*m_style_overrides);
    }
    return std::nullopt;
}

void widget::set_style(centui::style replacement)
{
    check_mutation();
    replacement.validate();
    auto overrides = std::make_unique<style_patch>(style_patch::replacement(replacement));
    m_style_overrides = std::move(overrides);
    commit_style(std::move(replacement));
}

bool widget::commit_style(centui::style replacement, bool notify)
{
    const auto changes = replacement.changes_from(m_style);
    if(!changes.any())
    {
        return false;
    }
    const bool targeting_changed = m_style.visibility != replacement.visibility ||
                                   m_style.overflow != replacement.overflow || m_style.display != replacement.display;
    m_style = std::move(replacement);
    if(changes.layout)
    {
        invalidate_layout();
    }
    else
    {
        invalidate_paint();
    }
    if(notify && targeting_changed)
    {
        interaction_changed();
    }
    return targeting_changed;
}

void widget::patch_style(const style_patch& patch)
{
    check_mutation();
    auto overrides = std::make_unique<style_patch>(m_style_overrides ? *m_style_overrides : style_patch{});
    overrides->merge(patch);
    auto context = m_document.lock();
    auto candidate = context ? resolve_style(
                                   context->owner.m_data->style_defaults,
                                   context->owner.m_data->style_patches,
                                   std::cref(*overrides)
                               )
                             : resolve_style({}, {}, std::cref(*overrides));
    m_style_overrides = std::move(overrides);
    commit_style(std::move(candidate));
}

void widget::clear_style_overrides()
{
    check_mutation();
    auto context = m_document.lock();
    auto candidate =
        context
            ? resolve_style(context->owner.m_data->style_defaults, context->owner.m_data->style_patches, std::nullopt)
            : resolve_style({}, {}, std::nullopt);
    m_style_overrides.reset();
    commit_style(std::move(candidate));
}

const layout_metrics& widget::metrics() const noexcept
{
    return m_metrics;
}

baseline_function widget::baseline()
{
    return {};
}

rect widget::bounds() const noexcept
{
    return m_bounds;
}

bool widget::enabled() const noexcept
{
    return m_enabled;
}

void widget::set_enabled(bool value)
{
    check_mutation();

    if(m_enabled == value)
    {
        return;
    }

    m_enabled = value;

    if(!value)
    {
        m_hovered = false;
        m_pressed = false;
    }

    invalidate_paint();
    interaction_changed();
}

widget::widget(centui::style_source value)
    : m_style_recipe(value.m_recipe)
{
    if(value.m_replacement)
    {
        m_style_overrides = std::make_unique<style_patch>(style_patch::replacement(*value.m_replacement));
    }
    m_style = resolve_style({}, {}, local_style());
}

size widget::measure(const measure_context&)
{
    return {};
}

void widget::paint(painter&) const
{
}

bool widget::on_event(const input_event&)
{
    return false;
}

void widget::on_focus_changed(bool)
{
}

void widget::on_layout()
{
}

std::optional<rect> widget::text_input_area() const
{
    return std::nullopt;
}

vec2 widget::child_offset() const noexcept
{
    return m_child_offset;
}

void widget::set_focus_scope(bool value)
{
    check_mutation();
    m_focus_scope = value;
}

void widget::set_interactive(bool value)
{
    check_mutation();

    if(m_interactive == value)
    {
        return;
    }

    m_interactive = value;
    interaction_changed();
}

void widget::set_focusable(bool value)
{
    check_mutation();

    if(m_focusable == value)
    {
        return;
    }

    m_focusable = value;
    interaction_changed();
}

void widget::set_clip_children(bool value)
{
    check_mutation();

    if(m_clip_children == value)
    {
        return;
    }

    m_clip_children = value;
    invalidate_paint();
    interaction_changed();
}

void widget::set_child_offset(vec2 value)
{
    check_mutation();

    if(!std::isfinite(value.x) || !std::isfinite(value.y))
    {
        throw std::invalid_argument("Child offsets must be finite");
    }

    if(m_child_offset == value)
    {
        return;
    }

    m_child_offset = value;
    if(auto context = m_document.lock())
    {
        context->placement_dirty = true;
        context->paint_dirty = true;
    }
}

void widget::check_mutation() const
{
    if(auto context = m_document.lock())
    {
        context->check_mutation();
    }
}

void widget::set_paint_boundary(bool value)
{
    check_mutation();
    m_paint_boundary = value;
    invalidate_paint();
}

void widget::invalidate_layout()
{
    check_mutation();
    m_measure_dirty = true;

    if(auto context = m_document.lock())
    {
        context->layout_dirty = true;
        context->paint_dirty = true;
    }
}

void widget::invalidate_paint()
{
    check_mutation();
    // Ancestors contain this widget; descendants may inherit its appearance.
    for(auto parent = m_parent.lock(); parent; parent = parent->m_parent.lock())
    {
        ++parent->m_paint_revision;
    }
    const auto dirty = [&](auto&& self, widget& value) -> void
    {
        ++value.m_paint_revision;
        for(const auto& child: value.m_children)
        {
            self(self, *child);
        }
    };
    dirty(dirty, *this);

    if(auto context = m_document.lock())
    {
        context->paint_dirty = true;
    }
}

namespace details
{

void widget_access::finish(const std::shared_ptr<widget>& parent)
{
    for(const auto& child: parent->m_children)
    {
        child->m_parent = parent;
    }
}

} // namespace details

void widget::validate_children(std::span<const handle<>> children) const
{
    std::vector<std::shared_ptr<widget>> seen;
    seen.reserve(children.size());

    for(const auto& child: children)
    {
        (void)child.get();
        auto value = child.m_value;

        if(value.get() == this || !value->m_parent_claim.expired() || !value->m_document.expired() ||
           std::ranges::find(seen, value) != seen.end())
        {
            throw std::invalid_argument("A child must be distinct, detached and unmounted");
        }

        for(auto ancestor = m_parent.lock(); ancestor; ancestor = ancestor->m_parent.lock())
        {
            if(ancestor == value)
            {
                throw std::invalid_argument("Child insertion would create a cycle");
            }
        }

        seen.push_back(value);
    }
}

void widget::insert_children(std::span<const handle<>> children)
{
    m_children.reserve(m_children.size() + children.size());

    for(const auto& child: children)
    {
        child.m_value->m_parent_claim = m_identity;
        child.m_value->m_parent = weak_from_this();
        m_children.push_back(child.m_value);
    }
}

void widget::interaction_changed()
{
    if(auto context = m_document.lock())
    {
        context->owner.interaction_changed();
    }
}

void widget::append(const handle<>& child)
{
    append(std::span{std::addressof(child), std::size_t{1}});
}

void widget::append(std::span<const handle<>> children)
{
    check_mutation();

    if(auto context = m_document.lock())
    {
        context->owner.append(handle<>{shared_from_this()}, children);
    }
    else
    {
        validate_children(children);
        insert_children(children);
    }
}

void widget::remove(const handle<>& child)
{
    check_mutation();
    (void)child.get();

    if(child.m_value->m_parent_claim.lock() != m_identity)
    {
        throw std::invalid_argument("Widget is not a child of this parent");
    }

    if(auto context = m_document.lock())
    {
        context->owner.remove(child);
    }
    else
    {
        std::erase(m_children, child.m_value);
        child.m_value->m_parent.reset();
        child.m_value->m_parent_claim.reset();
    }
}

std::vector<handle<>> widget::children() const
{
    std::vector<handle<>> result;
    result.reserve(m_children.size());

    for(const auto& child: m_children)
    {
        result.emplace_back(child);
    }

    return result;
}

handle<> widget::parent() const
{
    return handle<>{m_parent.lock()};
}

bool widget::request_focus()
{
    check_mutation();

    if(auto context = m_document.lock())
    {
        return context->owner.request_focus(handle<>{shared_from_this()});
    }

    return false;
}

bool widget::focus_next(bool reverse)
{
    check_mutation();

    if(auto context = m_document.lock())
    {
        return context->owner.advance_focus(handle<>{shared_from_this()}, reverse);
    }

    return false;
}

const host_services& widget::host() const
{
    if(auto context = m_document.lock(); context && context->owner.m_data->input.host)
    {
        return *context->owner.m_data->input.host;
    }

    static const host_services empty;

    return empty;
}

text_service& widget::fonts()
{
    if(auto context = m_document.lock())
    {
        return context->owner.fonts();
    }

    throw std::logic_error("A detached widget has no document text service");
}

void widget::on_frame(frame_duration)
{
}

void widget::request_frame()
{
    check_mutation();
    if(auto context = m_document.lock())
    {
        context->owner.request_frame(shared_from_this());
    }
    m_frame_requested = true;
}

} // namespace centui

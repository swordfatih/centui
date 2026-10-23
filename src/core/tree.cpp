#include <centui/tree.hpp>

namespace centui::details
{

[[nodiscard]] centui::color state_color(const states_color& colors, visual_state state) noexcept
{
    switch(state)
    {
    case visual_state::disabled:
    {
        return colors.disabled;
    }

    case visual_state::pressed:
    {
        return colors.pressed;
    }

    case visual_state::hovered:
    {
        return colors.hover;
    }

    case visual_state::focused:
    {
        return colors.focused;
    }

    default:
    {
        return colors.normal;
    }
    }
}

} // namespace centui::details

namespace centui
{

painter::painter(
    draw_list& output,
    text_service& fonts,
    rect content,
    rect clip,
    const centui::style& style,
    details::visual_state state
)
    : m_output(output),
      m_fonts(fonts),
      m_bounds(content),
      m_clip(clip),
      m_style(style),
      m_state(state)
{
}

rect painter::bounds() const noexcept
{
    return m_bounds;
}

text_service& painter::fonts() const noexcept
{
    return m_fonts;
}

centui::color painter::foreground() const noexcept
{
    return details::state_color(m_style.text_color, m_state);
}

void painter::rectangle(rect bounds, centui::color color)
{
    const auto clip = intersection(bounds, m_clip);

    if(color.a != 0 && clip.width > 0 && clip.height > 0)
    {
        m_output.commands.emplace_back(rectangle_command{bounds, color, m_clip});
    }
}

void painter::text(std::shared_ptr<const text_layout> layout)
{
    text(std::move(layout), m_bounds, details::state_color(m_style.text_color, m_state), m_style.text_align);
}

void painter::image(std::shared_ptr<const bitmap> value, rect bounds, centui::color tint)
{
    if(value)
    {
        validate_bitmap(*value);
        m_output.commands.emplace_back(image_command{bounds, m_clip, std::move(value), tint});
    }
}

void painter::text(std::string_view value)
{
    if(!value.empty())
    {
        text(m_fonts.prepare(value, m_style.font, m_style.font_size, std::max(0.0f, m_bounds.width)));
    }
}

void painter::text(
    std::shared_ptr<const text_layout> layout,
    rect bounds,
    centui::color color,
    text_alignment alignment
)
{
    const auto clip = intersection(bounds, m_clip);

    if(layout && color.a != 0 && clip.width > 0 && clip.height > 0)
    {
        m_output.commands.emplace_back(text_command{bounds, color, std::move(layout), alignment, clip});
    }
}

} // namespace centui

namespace centui::details
{

document_context::document_context(document& value)
    : owner(value)
{
}

void document_context::check_mutation() const
{
    if(preparing)
    {
        throw std::logic_error("Cannot mutate widgets during measurement or painting");
    }
}

} // namespace centui::details

namespace centui
{

const centui::style& widget::style() const noexcept
{
    return m_style;
}

const centui::style& widget::appearance() const noexcept
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
    if(m_style.visibility != visibility::visible)
    {
        return false;
    }

    for(auto ancestor = m_parent.lock(); ancestor; ancestor = ancestor->m_parent.lock())
    {
        if(ancestor->m_style.visibility != visibility::visible)
        {
            return false;
        }
    }

    return true;
}

void widget::set_style(centui::style replacement)
{
    check_mutation();
    replacement.validate();
    const auto changes = replacement.changes_from(m_style);

    if(!changes.layout && !changes.paint)
    {
        return;
    }

    const bool targeting_changed =
        m_style.visibility != replacement.visibility || m_style.overflow != replacement.overflow;
    m_style = std::move(replacement);

    if(changes.layout)
    {
        invalidate_layout();
    }
    else
    {
        invalidate_paint();
    }

    if(targeting_changed)
    {
        interaction_changed();
    }
}

void widget::patch_style(const style_patch& patch)
{
    set_style(patch.apply(m_style));
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

widget::widget(centui::style value)
    : m_style(std::move(value))
{
    m_style.validate();
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
    invalidate_layout();
}

void widget::check_mutation() const
{
    if(auto context = m_document.lock())
    {
        context->check_mutation();
    }
}

void widget::invalidate_layout()
{
    check_mutation();

    if(auto context = m_document.lock())
    {
        context->layout_dirty = true;
        context->paint_dirty = true;
    }
}

void widget::invalidate_paint()
{
    check_mutation();

    if(auto context = m_document.lock())
    {
        context->paint_dirty = true;
    }
}

} // namespace centui

namespace centui::details
{

void widget_access::finish(const std::shared_ptr<widget>& parent)
{
    for(const auto& child: parent->m_children)
    {
        child->m_parent = parent;
    }
}

} // namespace centui::details

namespace centui
{

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

document::document()
    : m_context(std::make_shared<details::document_context>(*this))
{
}

document::document(handle<> root)
    : document()
{
    replace_root(std::move(root));
}

document::~document()
{
    associate(m_root.m_value, {});
}

bool document::empty() const noexcept
{
    return !m_root.valid();
}

handle<> document::root() const noexcept
{
    return m_root;
}

text_service& document::fonts() noexcept
{
    return m_fonts;
}

void document::set_font(const centui::font& face)
{
    m_context->check_mutation();
    const auto revision = m_fonts.identity();
    m_fonts.set_font(face);

    if(revision != m_fonts.identity())
    {
        dirty_tree();
    }
}

void document::set_host_services(std::shared_ptr<const host_services> services)
{
    m_context->check_mutation();

    if(m_host == services)
    {
        return;
    }

    if(m_host && m_host->text_input)
    {
        m_host->text_input(std::nullopt);
    }

    m_host = std::move(services);
    m_text_input_area.reset();
    sync_host();
}

invalidation document::pending_changes() const noexcept
{
    return {.layout = m_context->layout_dirty, .paint = m_context->paint_dirty};
}

void document::mount(handle<> value)
{
    if(!empty())
    {
        throw std::logic_error("Document already has a root");
    }

    replace_root(std::move(value));
}

void document::replace_root(handle<> value)
{
    m_context->check_mutation();
    validate_root(value);
    edit(
        [this, value = std::move(value)]
        {
            validate_root(value);

            if(value == m_root)
            {
                return;
            }

            clear_input();
            associate(m_root.m_value, {});
            m_root = value;
            associate(m_root.m_value, m_context);
            dirty_tree();
        }
    );
}

void document::set_root(handle<> value)
{
    replace_root(std::move(value));
}

std::vector<handle<>> document::children(const handle<>& parent) const
{
    return checked(parent)->children();
}

handle<> document::parent(const handle<>& child) const
{
    return handle<>{checked(child)->m_parent.lock()};
}

void document::append(const handle<>& parent, const handle<>& child)
{
    append(parent, std::span{std::addressof(child), std::size_t{1}});
}

void document::append(const handle<>& parent, std::span<const handle<>> children)
{
    m_context->check_mutation();
    validate_append(parent, children);
    std::vector<handle<>> batch{children.begin(), children.end()};
    edit(
        [this, parent, batch = std::move(batch)]
        {
            validate_append(parent, batch);
            auto target = checked(parent);
            target->insert_children(batch);

            for(const auto& child: batch)
            {
                associate(child.m_value, m_context);
            }

            dirty_tree();
        }
    );
}

void document::remove(const handle<>& child)
{
    m_context->check_mutation();
    (void)checked(child);
    edit(
        [this, child]
        {
            auto target = checked(child);

            if(target == m_root.m_value)
            {
                m_root = {};
            }
            else
            {
                auto parent = target->m_parent.lock();
                std::erase(parent->m_children, target);
                target->m_parent.reset();
                target->m_parent_claim.reset();
            }

            associate(target, {});
            dirty_tree();
            synchronize_input();
        }
    );
}

const draw_list& document::prepare(centui::viewport viewport)
{
    m_context->check_mutation();
    process_events({}, viewport);

    if(viewport.width <= 0 || viewport.height <= 0)
    {
        m_draw.commands.clear();
        m_draw.viewport = viewport;

        return m_draw;
    }

    if(m_context->paint_dirty)
    {
        phase_guard guard{*m_context};
        draw_list candidate{viewport, {}};

        if(m_root.valid())
        {
            paint_tree(*m_root.m_value, candidate, {0, 0, viewport.logical_width(), viewport.logical_height()}, true);
        }

        m_draw = std::move(candidate);
        m_font_revision = m_fonts.identity();
        m_context->paint_dirty = false;
    }

    return m_draw;
}

void document::process_event(const input_event& event, centui::viewport viewport)
{
    process_events(std::span{std::addressof(event), std::size_t{1}}, viewport);
}

void document::process_events(std::span<const input_event> events, centui::viewport viewport)
{
    dispatch_guarded(
        [&]
        {
            settle_input(viewport);

            for(const auto& event: events)
            {
                dispatch(event);
                settle_input(viewport);
            }
        }
    );
}

void document::interaction_changed()
{
    m_hit_dirty = true;

    if(m_dispatching)
    {
        synchronize_input();
    }
    else
    {
        dispatch_guarded(
            [this]
            {
                synchronize_input();
            }
        );
    }
}

bool document::request_focus(const handle<>& target)
{
    m_context->check_mutation();

    if(!focus_eligible(checked(target)))
    {
        return false;
    }

    edit(
        [this, target]
        {
            if(focus_eligible(target.m_value))
            {
                set_focus(target.m_value);
            }
        }
    );

    return true;
}

bool document::advance_focus(const handle<>& scope, bool reverse)
{
    std::vector<handle<>> candidates;
    collect_focus(checked(scope), candidates);

    if(candidates.empty())
    {
        return false;
    }

    const auto found = std::ranges::find(candidates, handle<>{m_focus.lock()});
    std::size_t index = reverse ? candidates.size() - 1 : 0;

    if(found != candidates.end())
    {
        index = static_cast<std::size_t>(found - candidates.begin());
        index = reverse ? (index + candidates.size() - 1) % candidates.size() : (index + 1) % candidates.size();
    }

    return request_focus(candidates[index]);
}

void document::collect_focus(const std::shared_ptr<widget>& value, std::vector<handle<>>& result) const
{
    if(!value || !value->effectively_enabled() || !value->effectively_visible())
    {
        return;
    }

    if(focus_eligible(value))
    {
        result.emplace_back(value);
    }

    for(const auto& child: value->m_children)
    {
        collect_focus(child, result);
    }
}

void document::sync_host()
{
    const auto focused = m_focus.lock();
    const auto area = focused ? focused->text_input_area() : std::nullopt;

    if(area != m_text_input_area)
    {
        m_text_input_area = area;

        if(m_host && m_host->text_input)
        {
            m_host->text_input(area);
        }
    }
}

document::phase_guard::phase_guard(details::document_context& value)
    : context(value)
{
    context.check_mutation();
    context.preparing = true;
}

document::phase_guard::~phase_guard()
{
    context.preparing = false;
}

void document::associate(
    const std::shared_ptr<widget>& value,
    const std::shared_ptr<details::document_context>& context
) noexcept
{
    if(!value)
    {
        return;
    }

    value->m_document = context;

    if(!context)
    {
        value->m_hovered = false;
        value->m_pressed = false;
        value->m_focused = false;
    }

    for(const auto& child: value->m_children)
    {
        associate(child, context);
    }
}

std::shared_ptr<widget> document::checked(const handle<>& value) const
{
    (void)value.get();

    if(value.m_value->m_document.lock() != m_context)
    {
        throw std::invalid_argument("Widget does not belong to this document");
    }

    return value.m_value;
}

void document::validate_root(const handle<>& value) const
{
    if(!value.valid() || value == m_root)
    {
        return;
    }

    if(!value->m_parent_claim.expired() || !value->m_document.expired())
    {
        throw std::invalid_argument("Document root must be detached and unmounted");
    }
}

void document::validate_append(const handle<>& parent, std::span<const handle<>> children) const
{
    checked(parent)->validate_children(children);
}

void document::dirty_tree() noexcept
{
    m_hit_dirty = true;
    m_context->layout_dirty = true;
    m_context->paint_dirty = true;
}

void document::edit(std::function<void()> operation)
{
    if(m_dispatching)
    {
        m_edits.push_back(std::move(operation));
    }
    else
    {
        dispatch_guarded(operation);
    }
}

void document::flush_edits()
{
    std::exception_ptr failure;

    while(!m_edits.empty())
    {
        auto pending = std::exchange(m_edits, {});

        for(auto& operation: pending)
        {
            try
            {
                operation();
            }
            catch(...)
            {
                if(!failure)
                {
                    failure = std::current_exception();
                }
            }
        }
    }

    if(failure)
    {
        std::rethrow_exception(failure);
    }
}

void document::settle_input(centui::viewport viewport)
{
    do
    {
        flush_edits();
        update_layout(viewport);
        synchronize_input();
    } while(!m_edits.empty() || (m_context->layout_dirty && viewport.width > 0 && viewport.height > 0));
}

std::unique_ptr<document::layout_entry> document::make_layout(const std::shared_ptr<widget>& value)
{
    auto entry = std::make_unique<layout_entry>();
    entry->value = value;
    entry->native.update(value->m_style);

    if(value->m_children.empty())
    {
        entry->native.set_measure(
            [this, value](axis_constraint width, axis_constraint height)
            {
                const auto result = value->measure({width, height, m_fonts});

                if(!std::isfinite(result.width) || !std::isfinite(result.height) || result.width < 0 ||
                   result.height < 0)
                {
                    throw std::invalid_argument("Widget measurement must be finite and nonnegative");
                }

                return result;
            }
        );
    }
    else
    {
        for(const auto& child: value->m_children)
        {
            auto layout = make_layout(child);
            entry->native.insert_child(layout->native);
            entry->children.push_back(std::move(layout));
        }
    }

    return entry;
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
    auto box = entry.native.bounds();
    box.x += x;
    box.y += y;
    entry.value->m_bounds = box;
    auto content = entry.native.content_bounds();
    content.x += box.x;
    content.y += box.y;
    entry.value->m_content = content;

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

    m_fonts.set_scale(viewport.scale);

    if(m_font_revision != m_fonts.identity())
    {
        dirty_tree();
    }

    if(viewport != m_viewport)
    {
        if(viewport.scale != m_viewport.scale)
        {
            m_pointer_x *= m_viewport.scale / viewport.scale;
            m_pointer_y *= m_viewport.scale / viewport.scale;
            m_text_input_area.reset();
        }

        m_viewport = viewport;
        dirty_tree();
    }

    if(!m_context->layout_dirty || viewport.width <= 0 || viewport.height <= 0)
    {
        return;
    }

    {
        phase_guard guard{*m_context};

        if(m_root.valid())
        {
            auto tree = make_layout(m_root.m_value);
            tree->native.calculate(viewport.logical_width(), viewport.logical_height());
            check_measurements(*tree);
            store_bounds(*tree, 0, 0);
        }
    }

    m_context->layout_dirty = false;
    m_hit_dirty = true;
    m_context->paint_dirty = true;
    m_font_revision = m_fonts.identity();
    notify_layout(m_root.m_value);
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

void document::paint_tree(
    const widget& value,
    draw_list& output,
    rect clip,
    bool enabled,
    details::visual_state inherited_state
)
{
    if(value.m_style.visibility != visibility::visible)
    {
        return;
    }

    enabled = enabled && value.m_enabled;
    const auto state = !enabled               ? details::visual_state::disabled
                       : value.m_pressed      ? details::visual_state::pressed
                       : value.m_hovered      ? details::visual_state::hovered
                       : value.m_focused      ? details::visual_state::focused
                       : !value.m_interactive ? inherited_state
                                              : details::visual_state::normal;
    const auto appearance = value.resolved_style();
    painter background{output, m_fonts, value.m_bounds, clip, appearance, state};
    background.rectangle(value.m_bounds, details::state_color(value.m_style.background_color, state));
    const auto border = details::state_color(value.m_style.border_color, state);
    const auto width = std::min(value.m_style.border_width, std::min(value.m_bounds.width, value.m_bounds.height) / 2);

    if(width > 0)
    {
        const auto box = value.m_bounds;
        background.rectangle({box.x, box.y, box.width, width}, border);
        background.rectangle({box.x, box.y + box.height - width, box.width, width}, border);
        background.rectangle({box.x, box.y + width, width, box.height - 2 * width}, border);
        background.rectangle({box.x + box.width - width, box.y + width, width, box.height - 2 * width}, border);
    }

    painter content{output, m_fonts, value.m_content, intersection(clip, value.m_content), appearance, state};
    value.paint(content);
    const auto child_clip = value.m_clip_children || value.m_style.overflow != overflow::visible
                                ? intersection(clip, value.m_content)
                                : clip;

    for(const auto& child: value.m_children)
    {
        paint_tree(*child, output, child_clip, enabled, state);
    }
}

bool document::contains(rect box, float x, float y) noexcept
{
    return x >= box.x && y >= box.y && x < box.x + box.width && y < box.y + box.height;
}

bool document::eligible(const std::shared_ptr<widget>& value) const
{
    if(!value || !m_host_focused || m_viewport.width <= 0 || m_viewport.height <= 0 ||
       value->m_document.lock() != m_context || !value->m_interactive)
    {
        return false;
    }

    return value->effectively_enabled() && value->effectively_visible();
}

bool document::focus_eligible(const std::shared_ptr<widget>& value) const
{
    return value && m_host_focused && value->m_document.lock() == m_context && value->m_focusable &&
           value->effectively_enabled() && value->effectively_visible();
}

void document::set_focus(std::shared_ptr<widget> value)
{
    auto old = m_focus.lock();

    if(old == value)
    {
        return;
    }

    m_focus.reset();
    m_context->paint_dirty = true;

    if(old)
    {
        old->m_focused = false;
        old->on_focus_changed(false);
    }

    if(focus_eligible(value))
    {
        m_focus = value;
        value->m_focused = true;
        value->on_focus_changed(true);
    }
}

std::shared_ptr<widget> document::hit(const std::shared_ptr<widget>& value, float x, float y, rect clip) const
{
    if(!value || !value->m_enabled || value->m_style.visibility != visibility::visible || !contains(clip, x, y))
    {
        return {};
    }

    const auto child_clip = value->m_clip_children || value->m_style.overflow != overflow::visible
                                ? intersection(clip, value->m_content)
                                : clip;

    for(auto child = value->m_children.rbegin(); child != value->m_children.rend(); ++child)
    {
        if(auto target = hit(*child, x, y, child_clip))
        {
            return target;
        }
    }

    return eligible(value) && contains(value->m_bounds, x, y) ? value : nullptr;
}

std::shared_ptr<widget> document::hit(float x, float y) const
{
    return hit(m_root.m_value, x, y, {0, 0, m_viewport.logical_width(), m_viewport.logical_height()});
}

void document::route_motion(std::shared_ptr<widget> target, const input_event& event)
{
    for(auto current = std::move(target); current; current = current->m_parent.lock())
    {
        if(eligible(current) && current->on_event(event))
        {
            return;
        }
    }
}

bool document::route_event(std::shared_ptr<widget> target, const input_event& event)
{
    for(auto current = std::move(target); current; current = current->m_parent.lock())
    {
        if(current->effectively_enabled() && current->effectively_visible() && current->on_event(event))
        {
            return true;
        }
    }

    return false;
}

void document::set_hover(std::shared_ptr<widget> value)
{
    if(auto old = m_hover.lock(); old != value)
    {
        if(old)
        {
            old->m_hovered = false;
        }

        if(value)
        {
            value->m_hovered = true;
        }

        m_hover = value;
        m_context->paint_dirty = true;
    }
}

void document::cancel_capture()
{
    auto captured = m_capture.lock();
    m_capture.reset();

    if(captured)
    {
        captured->m_pressed = false;
        m_context->paint_dirty = true;
        captured->on_event({.type = input_event_type::pointer_cancelled, .button = m_capture_button});
    }
}

void document::clear_input()
{
    set_hover({});
    cancel_capture();
    set_focus({});
}

void document::synchronize_input()
{
    if(!eligible(m_hover.lock()))
    {
        set_hover({});
    }

    if(!eligible(m_capture.lock()))
    {
        cancel_capture();
    }

    if(auto focused = m_focus.lock(); focused && !focus_eligible(focused))
    {
        set_focus({});
    }

    if(m_hit_dirty && !m_context->layout_dirty)
    {
        m_hit_dirty = false;
        const bool inside = m_pointer_known && m_host_focused && m_pointer_x >= 0 && m_pointer_y >= 0 &&
                            m_pointer_x < m_viewport.logical_width() && m_pointer_y < m_viewport.logical_height();
        auto target = inside ? hit(m_pointer_x, m_pointer_y) : nullptr;
        set_hover(target);

        if(auto captured = m_capture.lock())
        {
            const bool pressed = captured == target;

            if(captured->m_pressed != pressed)
            {
                captured->m_pressed = pressed;
                m_context->paint_dirty = true;
            }
        }
    }
}

void document::dispatch(const input_event& event)
{
    if(event.type == input_event_type::key_pressed || event.type == input_event_type::key_released ||
       event.type == input_event_type::text_entered || event.type == input_event_type::composition)
    {
        if(!m_host_focused)
        {
            return;
        }

        if(!route_event(m_focus.lock(), event) && event.type == input_event_type::key_pressed && event.code == key::tab)
        {
            auto scope = m_root.m_value;

            for(auto current = m_focus.lock(); current; current = current->m_parent.lock())
            {
                if(current->m_focus_scope)
                {
                    scope = current;
                    break;
                }
            }

            if(scope)
            {
                advance_focus(handle<>{scope}, event.mods.shift);
            }
        }

        return;
    }

    m_hit_dirty = true;

    switch(event.type)
    {
    case input_event_type::focus_lost:
    {
        m_pointer_known = false;
        m_host_focused = false;
        clear_input();

        return;
    }

    case input_event_type::focus_gained:
    {
        m_host_focused = true;

        return;
    }

    case input_event_type::pointer_left:
    {
        m_pointer_known = false;
        set_hover({});

        if(auto captured = m_capture.lock())
        {
            captured->m_pressed = false;
            m_context->paint_dirty = true;
        }

        return;
    }

    case input_event_type::pointer_cancelled:
    {
        m_pointer_known = false;
        set_hover({});
        cancel_capture();

        return;
    }

    default:
    {
        break;
    }
    }

    m_pointer_known = true;
    m_pointer_x = event.x;
    m_pointer_y = event.y;
    const bool in_viewport =
        event.x >= 0 && event.y >= 0 && event.x < m_viewport.logical_width() && event.y < m_viewport.logical_height();
    auto target = in_viewport ? hit(event.x, event.y) : nullptr;
    set_hover(target);

    if(event.type == input_event_type::wheel)
    {
        route_event(target, event);

        return;
    }

    if(event.type == input_event_type::pointer_moved)
    {
        auto active = m_capture.lock();

        if(active)
        {
            const auto pressed = active == target;

            if(active->m_pressed != pressed)
            {
                active->m_pressed = pressed;
                m_context->paint_dirty = true;
            }
        }
        else
        {
            route_motion(target, event);

            return;
        }

        if(active)
        {
            active->on_event(event);
        }

        return;
    }

    if(event.type == input_event_type::pointer_pressed)
    {
        // One gesture at a time; extra presses cannot steal its capture.
        if(!m_capture.expired())
        {
            return;
        }

        if(target)
        {
            m_capture = target;
            m_capture_button = event.button;
            target->m_pressed = true;
            m_context->paint_dirty = true;

            if(event.button == mouse_button::primary && focus_eligible(target))
            {
                set_focus(target);
            }
            // Focus callbacks may disable the target and cancel the gesture.
            if(m_capture.lock() == target)
            {
                target->on_event(event);
            }
        }
    }
    else if(event.type == input_event_type::pointer_released)
    {
        if(event.button != m_capture_button)
        {
            return;
        }

        auto captured = m_capture.lock();
        // User code may mutate the tree or throw; capture is already ended.
        m_capture.reset();

        if(captured)
        {
            captured->m_pressed = false;
            m_context->paint_dirty = true;
            captured->on_event(event);
        }
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
    if(auto context = m_document.lock(); context && context->owner.m_host)
    {
        return *context->owner.m_host;
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

} // namespace centui

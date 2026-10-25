#include "document_state.hpp"

#include <algorithm>

namespace centui
{

namespace details
{

document_context::document_context(document& value)
    : owner(value)
{
}

} // namespace details

namespace details
{

void document_context::check_mutation() const
{
    if(preparing)
    {
        throw std::logic_error("Cannot mutate widgets during measurement or painting");
    }
}

} // namespace details

document::document()
    : m_data(std::make_unique<data>())
{
    m_data->context = std::make_shared<details::document_context>(*this);
    m_data->focus = std::make_unique<details::focus_controller>(*this);
}

document::document(handle<> root)
    : document()
{
    set_root(std::move(root));
}

document::~document()
{
    m_data->focus->abort(false);
    associate(m_data->root.m_value, {});
    const auto reset = [&](auto&& self, const std::shared_ptr<widget>& value) noexcept -> void
    {
        if(!value)
        {
            return;
        }
        try
        {
            value->commit_style(value->resolve_style({}, {}, value->local_style()), false);
        }
        catch(...)
        {
            // A destructor cannot report a throwing style definition; retain the last valid cache.
        }
        for(const auto& child: value->m_children)
        {
            self(self, child);
        }
    };
    reset(reset, m_data->root.m_value);
}

bool document::empty() const noexcept
{
    return !m_data->root.valid();
}

handle<> document::root() const noexcept
{
    return m_data->root;
}

text_service& document::fonts() noexcept
{
    return m_data->fonts;
}

void document::set_font(const centui::font& face)
{
    m_data->context->check_mutation();
    const auto revision = m_data->fonts.identity();
    m_data->fonts.set_font(face);

    if(revision != m_data->fonts.identity())
    {
        dirty_tree();
    }
}

void document::stage_styles(
    style_updates& updates,
    const std::shared_ptr<widget>& root,
    const style_patch& defaults,
    const details::style_patch_map& patches,
    std::optional<details::style_type_id> type
)
{
    if(!root)
    {
        return;
    }
    const auto layers = root->m_style_recipe.get().composition;
    if(!type || std::ranges::any_of(
                    layers,
                    [&](const auto& layer)
                    {
                        return layer.id == *type;
                    }
                ))
    {
        updates.emplace_back(root, root->resolve_style(defaults, patches, root->local_style()));
    }
    for(const auto& child: root->m_children)
    {
        stage_styles(updates, child, defaults, patches, type);
    }
}

bool document::commit_styles(style_updates& updates)
{
    bool targeting_changed = false;
    for(auto& [value, appearance]: updates)
    {
        targeting_changed |= value->commit_style(std::move(appearance), false);
    }
    return targeting_changed;
}

void document::update_styles(
    style_patch defaults,
    details::style_patch_map patches,
    std::optional<details::style_type_id> type
)
{
    m_data->context->check_mutation();
    style_updates updates;
    stage_styles(updates, m_data->root.m_value, defaults, patches, type);
    m_data->style_defaults = std::move(defaults);
    m_data->style_patches.swap(patches);
    if(commit_styles(updates))
    {
        interaction_changed();
    }
}

void document::set_style(const style_patch& defaults)
{
    update_styles(defaults, m_data->style_patches, std::nullopt);
}

void document::patch_style_type(details::style_type_id type, const style_patch& patch)
{
    auto patches = m_data->style_patches;
    patches[type].merge(patch);
    update_styles(m_data->style_defaults, std::move(patches), type);
}

void document::clear_style_type(details::style_type_id type)
{
    auto patches = m_data->style_patches;
    patches.erase(type);
    update_styles(m_data->style_defaults, std::move(patches), type);
}

void document::set_host_services(std::shared_ptr<const host_services> services)
{
    m_data->context->check_mutation();

    if(m_data->input.host == services)
    {
        return;
    }

    if(m_data->input.host && m_data->input.host->text_input)
    {
        m_data->input.host->text_input(std::nullopt);
    }

    m_data->input.host = std::move(services);
    m_data->input.text_input_area.reset();
    sync_host();
}

invalidation document::pending_changes() const noexcept
{
    const auto& context = *m_data->context;
    return {
        .layout = context.layout_dirty,
        .paint = context.paint_dirty || context.placement_dirty || context.geometry_dirty
    };
}

void document::set_root(handle<> value)
{
    m_data->context->check_mutation();
    validate_root(value);
    edit(
        [this, value = std::move(value)]
        {
            validate_root(value);

            if(value == m_data->root)
            {
                return;
            }

            style_updates detached;
            style_updates mounted;
            stage_styles(detached, m_data->root.m_value, {}, {});
            stage_styles(mounted, value.m_value, m_data->style_defaults, m_data->style_patches);
            clear_input();
            m_data->geometry.viewport_layout.reset();
            m_data->geometry.tree.reset();
            associate(m_data->root.m_value, {});
            m_data->root = value;
            associate(m_data->root.m_value, m_data->context);
            commit_styles(detached);
            commit_styles(mounted);
            dirty_tree();
        }
    );
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
    m_data->context->check_mutation();
    validate_append(parent, children);
    std::vector<handle<>> batch{children.begin(), children.end()};
    edit(
        [this, parent, batch = std::move(batch)]
        {
            validate_append(parent, batch);
            auto target = checked(parent);
            style_updates mounted;
            for(const auto& child: batch)
            {
                stage_styles(mounted, child.m_value, m_data->style_defaults, m_data->style_patches);
            }
            target->insert_children(batch);

            for(const auto& child: batch)
            {
                associate(child.m_value, m_data->context);
            }
            commit_styles(mounted);
            details::invalidation_access::invalidate(*target, details::change::topology);
        }
    );
}

void document::remove(const handle<>& child)
{
    m_data->context->check_mutation();
    (void)checked(child);
    edit(
        [this, child]
        {
            auto target = checked(child);
            style_updates detached;
            stage_styles(detached, target, {}, {});
            // Layout callbacks retain widgets; release them when the edit commits,
            // even when a zero-sized viewport prevents the next layout pass.
            if(target == m_data->root.m_value)
            {
                m_data->geometry.viewport_layout.reset();
                m_data->geometry.tree.reset();
            }
            else if(m_data->geometry.tree)
            {
                const auto detach = [&](auto&& self, layout_entry& entry) -> bool
                {
                    for(auto it = entry.children.begin(); it != entry.children.end(); ++it)
                    {
                        if((*it)->value == target)
                        {
                            entry.native.remove_child((*it)->native);
                            entry.children.erase(it);
                            return true;
                        }
                        if(self(self, **it))
                        {
                            return true;
                        }
                    }
                    return false;
                };
                detach(detach, *m_data->geometry.tree);
            }

            if(target == m_data->root.m_value)
            {
                m_data->root = {};
            }
            else
            {
                auto parent = target->m_parent.lock();
                std::erase(parent->m_children, target);
                details::invalidation_access::invalidate(*parent, details::change::topology);
                target->m_parent.reset();
                target->m_parent_claim.reset();
            }

            associate(target, {});
            commit_styles(detached);
            dirty_tree();
            synchronize_input();
        }
    );
}

const draw_list& document::prepare(centui::viewport viewport)
{
    m_data->context->check_mutation();
    process_events({}, viewport);

    if(viewport.width <= 0 || viewport.height <= 0)
    {
        m_data->draw.commands.clear();
        m_data->draw.viewport = viewport;

        return m_data->draw;
    }

    if(m_data->context->paint_dirty)
    {
        phase_guard guard{*m_data->context};
        draw_list candidate{viewport, {}};

        if(m_data->root.valid())
        {
            paint_tree(
                *m_data->root.m_value,
                candidate,
                {0, 0, viewport.logical_width(), viewport.logical_height()},
                true
            );
        }

        m_data->draw = std::move(candidate);
        m_data->font_revision = m_data->fonts.identity();
        m_data->context->paint_dirty = false;
    }

    return m_data->draw;
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
    value->m_modal_pending = context && value->m_modal_open;
    value->m_layout_dirty = true;
    value->m_layout_descendants_dirty = true;
    value->m_geometry_dirty = true;
    if(context)
    {
        context->owner.m_data->scheduler.frames_dirty = true;
    }

    if(!context)
    {
        value->m_hovered = false;
        value->m_pressed = false;
        // Focus mirrors are cleared by the controller after detachment.
    }

    for(const auto& child: value->m_children)
    {
        associate(child, context);
    }
}

std::shared_ptr<widget> document::checked(const handle<>& value) const
{
    (void)value.get();

    if(value.m_value->m_document.lock() != m_data->context)
    {
        throw std::invalid_argument("Widget does not belong to this document");
    }

    return value.m_value;
}

void document::validate_root(const handle<>& value) const
{
    if(!value.valid() || value == m_data->root)
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
    m_data->input.hit_dirty = true;
    details::invalidation_access::invalidate_document(*m_data->context, details::change::topology);
}

void document::edit(std::function<void()> operation)
{
    if(m_data->dispatching)
    {
        m_data->edits.push_back(std::move(operation));
    }
    else
    {
        dispatch_guarded(operation);
    }
}

void document::flush_edits()
{
    std::exception_ptr failure;

    while(!m_data->edits.empty())
    {
        auto pending = std::exchange(m_data->edits, {});

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

void document::settle_geometry(centui::viewport viewport)
{
    do
    {
        flush_edits();
        update_layout(viewport);
        auto& context = *m_data->context;
        if(viewport.width > 0 && viewport.height > 0 && context.geometry_dirty)
        {
            context.geometry_dirty = false;
            refresh_geometry(m_data->root.m_value);
        }
        // Eligibility/cancellation also runs on minimized surfaces. Hit testing
        // itself waits for geometry and placement to settle.
        synchronize_input();
    } while(!m_data->edits.empty() ||
            ((m_data->context->layout_dirty || m_data->context->placement_dirty || m_data->context->geometry_dirty) &&
             viewport.width > 0 && viewport.height > 0));
}

void document::settle_input(centui::viewport viewport)
{
    if(viewport != m_data->viewport)
    {
        for(auto& request: m_data->reveals)
        {
            request.parked = false;
        }
    }
    settle_geometry(viewport);
    while(settle_reveals())
    {
        settle_geometry(viewport);
    }
}

void document::dispatch_guarded(const std::function<void()>& operation)
{
    m_data->context->check_mutation();

    if(m_data->dispatching)
    {
        throw std::logic_error("Recursive event dispatch is not allowed");
    }

    m_data->dispatching = true;
    m_data->reveal_restarts.clear();
    const auto pending_work = [&]
    {
        return !m_data->edits.empty() || std::ranges::any_of(
                                             m_data->reveals,
                                             [](const auto& request)
                                             {
                                                 return !request.parked;
                                             }
                                         );
    };

    try
    {
        operation();
        do
        {
            settle_input(m_data->viewport);
            sync_host();
        } while(pending_work());
        m_data->focus->reset_transaction();
        m_data->dispatching = false;
    }
    catch(...)
    {
        const auto failure = std::current_exception();

        try
        {
            do
            {
                try
                {
                    flush_edits();
                }
                catch(...)
                {
                }
                synchronize_input();
            } while(!m_data->edits.empty());
        }
        catch(...)
        {
            // Callback exceptions retain priority over failures applying edits.
        }

        m_data->focus->reset_transaction();
        m_data->dispatching = false;

        std::rethrow_exception(failure);
    }
}

preparation_statistics document::statistics() const noexcept
{
    return m_data->statistics;
}

void document::set_command_cache_budget(std::size_t bytes)
{
    m_data->context->check_mutation();
    m_data->command_budget = bytes;
    m_data->invalidate_all_commands();
}

void document::set_paint_cache_budget(std::size_t bytes)
{
    m_data->context->check_mutation();
    m_data->drawings.set_cache_budget(bytes);
}

void document::request_frame(const std::shared_ptr<widget>& value)
{
    m_data->scheduler.request(value);
}

void document::frame_scheduler::request(const std::shared_ptr<widget>& value)
{
    std::erase_if(
        frames,
        [](const auto& entry)
        {
            return entry.expired();
        }
    );
    if(std::none_of(
           frames.begin(),
           frames.end(),
           [&](const auto& entry)
           {
               return entry.lock() == value;
           }
       ))
    {
        frames.push_back(value);
    }
}

void document::advance(frame_duration elapsed)
{
    if(!std::isfinite(elapsed.count()) || elapsed.count() < 0)
    {
        throw std::invalid_argument("Frame duration must be finite and nonnegative");
    }
    dispatch_guarded(
        [&]
        {
            if(m_data->scheduler.frames_dirty)
            {
                const auto collect = [&](auto&& self, const std::shared_ptr<widget>& value) -> void
                {
                    if(!value)
                    {
                        return;
                    }
                    if(value->m_frame_requested)
                    {
                        request_frame(value);
                    }
                    for(const auto& child: value->m_children)
                    {
                        self(self, child);
                    }
                };
                collect(collect, m_data->root.m_value);
                m_data->scheduler.frames_dirty = false;
            }
            auto pending = std::exchange(m_data->scheduler.frames, {});
            std::size_t next = 0;
            try
            {
                for(; next < pending.size(); ++next)
                {
                    auto value = pending[next].lock();
                    if(value && value->m_document.lock() == m_data->context && value->m_frame_requested)
                    {
                        value->m_frame_requested = false;
                        value->on_frame(elapsed);
                    }
                }
            }
            catch(...)
            {
                for(; next < pending.size(); ++next)
                {
                    if(auto value = pending[next].lock(); value && value->m_frame_requested)
                    {
                        request_frame(value);
                    }
                }
                throw;
            }
        }
    );
}

} // namespace centui

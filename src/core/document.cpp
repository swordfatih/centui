#include "document_state.hpp"

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
}

document::document(handle<> root)
    : document()
{
    set_root(std::move(root));
}

document::~document()
{
    associate(m_data->root.m_value, {});
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
    return {.layout = m_data->context->layout_dirty, .paint = m_data->context->paint_dirty};
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

            clear_input();
            m_data->geometry.viewport_layout.reset();
            m_data->geometry.tree.reset();
            associate(m_data->root.m_value, {});
            m_data->root = value;
            associate(m_data->root.m_value, m_data->context);
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
            target->insert_children(batch);

            for(const auto& child: batch)
            {
                associate(child.m_value, m_data->context);
            }

            dirty_tree();
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
    if(context)
    {
        context->owner.m_data->scheduler.frames_dirty = true;
    }

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
    m_data->context->placement_dirty = true;
    m_data->context->layout_dirty = true;
    m_data->context->paint_dirty = true;
    m_data->clear_commands();
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

void document::settle_input(centui::viewport viewport)
{
    do
    {
        flush_edits();
        update_layout(viewport);
        synchronize_input();
    } while(!m_data->edits.empty() || ((m_data->context->layout_dirty || m_data->context->placement_dirty) &&
                                       viewport.width > 0 && viewport.height > 0));
}

void document::dispatch_guarded(const std::function<void()>& operation)
{
    m_data->context->check_mutation();

    if(m_data->dispatching)
    {
        throw std::logic_error("Recursive event dispatch is not allowed");
    }

    m_data->dispatching = true;

    try
    {
        operation();
        settle_input(m_data->viewport);
        sync_host();
        m_data->dispatching = false;
    }
    catch(...)
    {
        const auto failure = std::current_exception();

        try
        {
            do
            {
                flush_edits();
                synchronize_input();
            } while(!m_data->edits.empty());
        }
        catch(...)
        {
            // Callback exceptions retain priority over failures applying edits.
        }

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
    m_data->clear_commands();
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

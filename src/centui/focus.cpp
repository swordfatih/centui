#include "document_state.hpp"

#include <algorithm>
#include <stdexcept>

namespace centui::details
{

focus_controller::focus_controller(document& owner)
    : m_owner(owner)
{
}

std::shared_ptr<widget> focus_controller::current() const
{
    return m_current.lock();
}

std::shared_ptr<widget> focus_controller::modal() const
{
    return m_modals.empty() ? nullptr : m_modals.back().root.lock();
}

bool focus_controller::active() const noexcept
{
    return m_active;
}

bool focus_controller::show_focus_indication() const noexcept
{
    return m_show_focus_indication;
}

void focus_controller::set_show_focus_indication(bool value)
{
    if(m_suppressed || m_show_focus_indication == value)
    {
        return;
    }
    m_show_focus_indication = value;
    if(auto focused = current(); focused && focused->focus_active())
    {
        focused->invalidate_paint();
    }
}

bool focus_controller::suppressed() const noexcept
{
    return m_suppressed;
}

bool focus_controller::descendant(const std::shared_ptr<widget>& value, const std::shared_ptr<widget>& root)
{
    for(auto node = value; node; node = node->m_parent.lock())
    {
        if(node == root)
        {
            return true;
        }
    }
    return false;
}

bool focus_controller::allowed(const std::shared_ptr<widget>& value) const
{
    return value && (m_modals.empty() || descendant(value, modal()));
}

bool focus_controller::mounted_visible(const std::shared_ptr<widget>& value) const
{
    return value && value->m_document.lock() == m_owner.m_data->context && value->effectively_enabled() &&
           value->effectively_visible();
}

bool focus_controller::eligible(const std::shared_ptr<widget>& value) const
{
    return mounted_visible(value) && allowed(value) && value->m_focusable &&
           value->m_style.display != display::contents;
}

void focus_controller::count_transition()
{
    if(++m_transitions > 64)
    {
        abort();
        throw std::logic_error("Focus transaction exceeded 64 transitions");
    }
}

void focus_controller::activity(const std::shared_ptr<widget>& value, bool active)
{
    if(value && value->m_focus_active != active)
    {
        value->m_focus_active = active;
        value->invalidate_paint();
        if(!active)
        {
            m_owner.sync_host();
        }
        value->on_focus_activity_changed(active);
    }
}

void focus_controller::transfer(std::shared_ptr<widget> value)
{
    if(m_suppressed || m_notifying || (value && !eligible(value)) || current() == value)
    {
        return;
    }
    count_transition();
    std::erase_if(
        m_owner.m_data->reveals,
        [&](const auto& entry)
        {
            return entry.focus_driven && entry.target.lock() != value;
        }
    );
    m_notifying = true;
    try
    {
        if(auto old = current())
        {
            activity(old, false);
            old->m_focused = false;
            m_current.reset();
            old->invalidate_paint();
            old->on_focus_changed(false);
        }

        if(eligible(value))
        {
            m_current = value;
            value->m_focused = true;
            value->invalidate_paint();
            value->on_focus_changed(true);
            if(eligible(value) && m_active)
            {
                activity(value, true);
            }
            if(eligible(value))
            {
                m_owner.request_reveal(value, std::nullopt, true);
            }
        }
        m_notifying = false;
    }
    catch(...)
    {
        abort();
        throw;
    }
}

bool focus_controller::request(const std::shared_ptr<widget>& value)
{
    if(m_suppressed || !eligible(value))
    {
        return false;
    }
    m_owner.edit(
        [this, target = std::weak_ptr<widget>{value}]
        {
            if(auto value = target.lock(); !m_suppressed && eligible(value))
            {
                transfer(value);
            }
        }
    );
    return true;
}

void focus_controller::clear()
{
    if(!m_suppressed)
    {
        m_owner.edit(
            [this]
            {
                transfer({});
            }
        );
    }
}

void focus_controller::collect(const std::shared_ptr<widget>& root, std::vector<std::shared_ptr<widget>>& result) const
{
    if(!mounted_visible(root))
    {
        return;
    }
    if(eligible(root))
    {
        result.push_back(root);
    }
    for(const auto& child: root->m_children)
    {
        collect(child, result);
    }
}

std::shared_ptr<widget> focus_controller::first(const std::shared_ptr<widget>& root) const
{
    std::vector<std::shared_ptr<widget>> candidates;
    // Modal contents precede the root fallback.
    if(root)
    {
        for(const auto& child: root->m_children)
        {
            collect(child, candidates);
        }
    }
    return candidates.empty() ? (eligible(root) ? root : nullptr) : candidates.front();
}

bool focus_controller::next(bool reverse)
{
    set_show_focus_indication(true);
    std::vector<std::shared_ptr<widget>> candidates;
    const auto root = m_owner.m_data->root;
    collect(m_modals.empty() ? (root.valid() ? root.operator->() : nullptr) : modal(), candidates);
    if(candidates.empty())
    {
        return false;
    }
    const auto found = std::ranges::find(candidates, current());
    auto index = reverse ? candidates.size() - 1 : 0;
    if(found != candidates.end())
    {
        index = static_cast<std::size_t>(found - candidates.begin());
        index = reverse ? (index + candidates.size() - 1) % candidates.size() : (index + 1) % candidates.size();
    }
    return request(candidates[index]);
}

void focus_controller::validate_modal(const std::shared_ptr<widget>& root) const
{
    if(!mounted_visible(root))
    {
        throw std::invalid_argument("Modal root must be mounted, visible and enabled");
    }
    if(root == modal())
    {
        return;
    }
    if(std::ranges::any_of(
           m_modals,
           [&](const auto& entry)
           {
               return entry.root.lock() == root;
           }
       ) ||
       (!m_modals.empty() && !descendant(root, modal())))
    {
        throw std::invalid_argument("Nested modal must descend from the active modal");
    }
}

void focus_controller::enter(const std::shared_ptr<widget>& root, const std::shared_ptr<widget>& initial)
{
    if(m_suppressed)
    {
        return;
    }
    validate_modal(root);
    m_owner.edit(
        [this, root = std::weak_ptr<widget>{root}, initial = std::weak_ptr<widget>{initial}]
        {
            if(m_suppressed)
            {
                return;
            }
            auto value = root.lock();
            validate_modal(value);
            if(modal() == value)
            {
                return;
            }
            count_transition();
            m_modals.push_back({value, current()});
            cancel_outside();
            auto target = initial.lock();
            transfer(eligible(target) ? target : first(value));
        }
    );
}

void focus_controller::cancel_outside()
{
    m_owner.m_data->input.hit_dirty = true;
    if(!allowed(m_owner.m_data->input.capture.lock()))
    {
        m_owner.cancel_capture();
    }
    if(!allowed(m_owner.m_data->input.hover.lock()))
    {
        m_owner.set_hover({});
    }
}

void focus_controller::unwind(std::size_t index)
{
    count_transition();
    auto restore = m_modals[index].restore.lock();
    m_modals.erase(m_modals.begin() + index, m_modals.end());
    cancel_outside();
    transfer(eligible(restore) ? restore : first(modal()));
}

void focus_controller::leave(const std::shared_ptr<widget>& root)
{
    if(!root || m_suppressed)
    {
        return;
    }
    m_owner.edit(
        [this, root = std::weak_ptr<widget>{root}]
        {
            if(m_suppressed)
            {
                return;
            }
            for(std::size_t i = 0; i < m_modals.size(); ++i)
            {
                if(m_modals[i].root.lock() == root.lock())
                {
                    unwind(i);
                    return;
                }
            }
        }
    );
}

void focus_controller::activate(bool value)
{
    if(m_suppressed || m_active == value)
    {
        return;
    }
    count_transition();
    m_active = value;
    try
    {
        reconcile();
    }
    catch(...)
    {
        abort();
        throw;
    }
}

void focus_controller::reconcile()
{
    if(m_notifying)
    {
        return;
    }
    for(std::size_t i = 0; i < m_modals.size(); ++i)
    {
        if(!mounted_visible(m_modals[i].root.lock()))
        {
            if(m_suppressed)
            {
                m_modals.erase(m_modals.begin() + i, m_modals.end());
            }
            else
            {
                unwind(i);
            }
            break;
        }
    }
    if(m_suppressed)
    {
        return;
    }
    if(auto value = current())
    {
        if(!eligible(value))
        {
            transfer({});
        }
        else if(value->m_focus_active != m_active)
        {
            m_notifying = true;
            try
            {
                activity(value, m_active);
                m_notifying = false;
            }
            catch(...)
            {
                abort();
                throw;
            }
        }
    }
    // Detached popups keep intent; mounting activates them without a geometry hook.
    const auto scan = [&](auto&& self, const std::shared_ptr<widget>& value) -> void
    {
        if(!mounted_visible(value))
        {
            return;
        }
        if(value->m_modal_pending && !std::ranges::any_of(
                                         m_modals,
                                         [&](const auto& entry)
                                         {
                                             return entry.root.lock() == value;
                                         }
                                     ))
        {
            // Only one registration is queued per settlement pass.
            value->m_modal_pending = false;
            enter(value, {});
        }
        for(const auto& child: value->m_children)
        {
            self(self, child);
        }
    };
    const auto root = m_owner.m_data->root;
    scan(scan, root.valid() ? root.operator->() : nullptr);
}

void focus_controller::abort(bool cancel_interaction) noexcept
{
    m_suppressed = true;
    m_notifying = false;
    auto value = current();
    m_current.reset();
    if(value)
    {
        value->m_focused = false;
        value->m_focus_active = false;
        try
        {
            value->invalidate_paint();
        }
        catch(...)
        {
        }
        if(cancel_interaction && m_owner.m_data->input.capture.lock() != value)
        {
            // Cancellation is not a focus notification and must never activate a control.
            try
            {
                value->on_event({.type = input_event_type::pointer_cancelled});
            }
            catch(...)
            {
            }
        }
    }
    std::erase_if(
        m_owner.m_data->reveals,
        [](const auto& entry)
        {
            return entry.focus_driven;
        }
    );
    if(cancel_interaction)
    {
        try
        {
            m_owner.set_hover({});
        }
        catch(...)
        {
        }
        try
        {
            m_owner.cancel_capture();
        }
        catch(...)
        {
        }
    }
    m_owner.m_data->input.text_input_area.reset();
    try
    {
        const auto& host = m_owner.m_data->input.host;
        if(host && host->text_input)
        {
            host->text_input(std::nullopt);
        }
    }
    catch(...)
    {
    }
}

void focus_controller::reset_transaction() noexcept
{
    m_transitions = 0;
    m_suppressed = false;
}

} // namespace centui::details

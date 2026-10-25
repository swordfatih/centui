#include "document_state.hpp"

#include <algorithm>

namespace centui
{

void document::request_reveal(const std::shared_ptr<widget>& target, std::optional<rect> local, bool focus)
{
    m_data->context->check_mutation();
    if(!target || target->m_document.lock() != m_data->context || (focus && m_data->focus->suppressed()))
    {
        return;
    }
    auto& requests = m_data->reveals;
    const auto found = std::ranges::find_if(
        requests,
        [&](const auto& entry)
        {
            return entry.target.lock() == target;
        }
    );
    if(found != requests.end())
    {
        // A focus transition must not overwrite an explicit caret/rectangle request.
        if(!focus || found->focus_driven)
        {
            found->local = local;
            found->focus_driven = focus;
            found->parked = false;
        }
    }
    else
    {
        requests.push_back({target, local, focus});
    }
    if(!m_data->dispatching)
    {
        dispatch_guarded([] {});
    }
}

bool document::settle_reveals()
{
    if(m_data->revealing)
    {
        return false;
    }
    m_data->revealing = true;

    struct reveal_guard
    {
        bool& value;

        ~reveal_guard()
        {
            value = false;
        }
    } guard{m_data->revealing};

    auto pending = std::exchange(m_data->reveals, {});
    bool processed = false;
    for(auto& request: pending)
    {
        auto target = request.target.lock();
        if(!target || target->m_document.lock() != m_data->context ||
           (request.focus_driven && (m_data->focus->suppressed() || m_data->focus->current() != target)))
        {
            continue;
        }
        auto& restarts = m_data->reveal_restarts[request.target];
        if(restarts >= 8)
        {
            continue;
        }
        if(request.parked || m_data->viewport.width <= 0 || m_data->viewport.height <= 0)
        {
            request.parked = true;
            m_data->reveals.push_back(request);
            continue;
        }
        processed = true;
        bool parked = false;
        bool restart = false;
        do
        {
            restart = false;
            const auto target_bounds = [&]
            {
                auto box = request.local.value_or(target->focus_bounds());
                if(request.local)
                {
                    box.x += target->bounds().x;
                    box.y += target->bounds().y;
                }
                return box;
            };
            auto carried = target_bounds();
            auto expected = carried;

            struct ancestor_snapshot
            {
                std::weak_ptr<widget> value;
                rect bounds;
                rect content;
                bool clips;
            };

            std::vector<ancestor_snapshot> ancestors;
            for(auto ancestor = target->m_parent.lock(); ancestor; ancestor = ancestor->m_parent.lock())
            {
                ancestors.push_back(
                    {ancestor,
                     ancestor->bounds(),
                     ancestor->content_bounds(),
                     ancestor->m_style.display != display::contents &&
                         (ancestor->m_clip_children || ancestor->m_style.overflow != overflow::visible)}
                );
            }
            for(std::size_t i = 0; i < ancestors.size(); ++i)
            {
                auto ancestor = ancestors[i].value.lock();
                if(!ancestor || target->m_document.lock() != m_data->context)
                {
                    break;
                }
                const bool clips = ancestor->m_style.display != display::contents &&
                                   (ancestor->m_clip_children || ancestor->m_style.overflow != overflow::visible);
                if(clips && (ancestor->content_bounds().width <= 0 || ancestor->content_bounds().height <= 0))
                {
                    parked = true;
                    break;
                }

                const auto displacement = ancestor->reveal_descendant(carried);
                carried.x += displacement.x;
                carried.y += displacement.y;
                expected.x += displacement.x;
                expected.y += displacement.y;
                for(std::size_t inner = 0; inner < i; ++inner)
                {
                    ancestors[inner].bounds.x += displacement.x;
                    ancestors[inner].bounds.y += displacement.y;
                    ancestors[inner].content.x += displacement.x;
                    ancestors[inner].content.y += displacement.y;
                }
                // Placement, hooks and edits settle without recursively draining reveals.
                settle_geometry(m_data->viewport);
                if(target->m_document.lock() != m_data->context ||
                   (request.focus_driven && (m_data->focus->suppressed() || m_data->focus->current() != target)))
                {
                    break;
                }
                auto parent = target->m_parent.lock();
                for(const auto& snapshot: ancestors)
                {
                    const bool parent_clips =
                        parent && parent->m_style.display != display::contents &&
                        (parent->m_clip_children || parent->m_style.overflow != overflow::visible);
                    if(!parent || parent != snapshot.value.lock() || parent->bounds() != snapshot.bounds ||
                       parent->content_bounds() != snapshot.content || parent_clips != snapshot.clips)
                    {
                        restart = true;
                        break;
                    }
                    parent = parent->m_parent.lock();
                }
                restart = restart || parent != nullptr || target_bounds() != expected;
                if(restart)
                {
                    break;
                }

                if(ancestor->m_style.display != display::contents &&
                   (ancestor->m_clip_children || ancestor->m_style.overflow != overflow::visible))
                {
                    const auto clip = ancestor->content_bounds();
                    if(clip.width <= 0 || clip.height <= 0)
                    {
                        parked = true;
                        break;
                    }
                    carried = intersection(carried, clip);
                    if(carried.width <= 0 || carried.height <= 0)
                    {
                        break;
                    }
                }
            }
        } while(restart && ++restarts < 8 && target->m_document.lock() == m_data->context);

        // Placement hooks can repeat the request currently being resolved.
        // Different explicit rectangles remain queued and share the restart budget.
        std::erase_if(
            m_data->reveals,
            [&](const auto& entry)
            {
                return entry.target.lock() == target && (restarts >= 8 || entry.local == request.local);
            }
        );

        if(parked && target->m_document.lock() == m_data->context &&
           !(request.focus_driven && (m_data->focus->suppressed() || m_data->focus->current() != target)) &&
           !std::ranges::any_of(
               m_data->reveals,
               [&](const auto& entry)
               {
                   return entry.target.lock() == target;
               }
           ))
        {
            request.parked = true;
            m_data->reveals.push_back(request);
        }
    }
    return processed;
}

} // namespace centui

#include "document_state.hpp"

namespace centui
{

event::event(centui::document& owner, handle<> target, input_reference input)
    : m_document(owner),
      m_target(std::move(target)),
      m_input(input)
{
}

centui::document& event::document() const noexcept
{
    return m_document;
}

handle<> event::target() const noexcept
{
    return m_target;
}

event::input_reference event::input() const noexcept
{
    return m_input;
}

centui::event details::event_access::make(widget& target)
{
    auto context = target.m_document.lock();
    if(!context)
    {
        throw std::logic_error("An event-aware callback requires a mounted widget");
    }
    return centui::event{context->owner, handle<>{target.shared_from_this()}, context->owner.m_data->active_input};
}

} // namespace centui

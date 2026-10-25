#pragma once

#include <functional>
#include <optional>

#include <centui/handle.hpp>
#include <centui/types.hpp>

namespace centui
{

namespace details
{

class event_access;

}

/// \brief Callback context. Borrowed document and input are valid during invocation only.
class event
{
public:
    using input_reference = std::optional<std::reference_wrapper<const input_event>>;

    [[nodiscard]] centui::document& document() const noexcept;

    /// \brief The widget emitting this callback, rather than an internal child.
    [[nodiscard]] handle<> target() const noexcept;

    /// \brief Empty when there is no originating input, such as a programmatic close.
    [[nodiscard]] input_reference input() const noexcept;

private:
    friend class details::event_access;

    event(centui::document& owner, handle<> target, input_reference input);

    centui::document& m_document;
    handle<> m_target;
    input_reference m_input;
};

namespace details
{

class event_access
{
public:
    static centui::event make(widget& target);
};

} // namespace details

} // namespace centui

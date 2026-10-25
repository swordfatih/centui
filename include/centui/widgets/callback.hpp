#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <centui/event.hpp>

namespace centui
{

/// \brief Accepts callbacks with optional leading event&, preserving running invocations during replacement.
template <typename... Args>
class callback
{
public:
    callback() = default;

    callback(std::nullptr_t)
    {
    }

    template <typename F>
        requires(!std::same_as<std::remove_cvref_t<F>, callback>) &&
                (std::is_invocable_r_v<void, F&, event&, Args...> || std::is_invocable_r_v<void, F&, Args...>)
    callback(F value)
    {
        if constexpr(requires { static_cast<bool>(value); })
        {
            if(!static_cast<bool>(value))
            {
                return;
            }
        }
        m_value = std::make_shared<function>(
            [value = std::move(value)](widget& target, Args... args) mutable
            {
                if constexpr(std::is_invocable_r_v<void, F&, event&, Args...>)
                {
                    auto context = details::event_access::make(target);
                    std::invoke(value, context, std::forward<Args>(args)...);
                }
                else
                {
                    std::invoke(value, std::forward<Args>(args)...);
                }
            }
        );
    }

    void set(callback value)
    {
        m_value = std::move(value.m_value);
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>(m_value);
    }

    void operator()(widget& target, Args... args) const
    {
        auto value = m_value;
        if(value)
        {
            (*value)(target, std::forward<Args>(args)...);
        }
    }

private:
    using function = std::function<void(widget&, Args...)>;
    std::shared_ptr<function> m_value;
};

} // namespace centui

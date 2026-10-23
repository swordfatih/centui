#pragma once

#include <functional>
#include <memory>
#include <utility>

namespace centui::details
{

/// \brief Shares callback storage so a running callback can safely replace itself.
template <typename... Args>
class callback
{
public:
    void set(std::function<void(Args...)> value)
    {
        m_value = value ? std::make_shared<std::function<void(Args...)>>(std::move(value)) : nullptr;
    }

    void operator()(Args... args) const
    {
        auto value = m_value;

        if(value)
        {
            (*value)(std::forward<Args>(args)...);
        }
    }

private:
    std::shared_ptr<std::function<void(Args...)>> m_value;
};

} // namespace centui::details

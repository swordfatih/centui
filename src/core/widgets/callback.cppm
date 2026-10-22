module;

#include <functional>
#include <memory>
#include <utility>

export module centui.widgets.callback;

export namespace centui::details
{
template <typename... Args>
class callback
{
public:
    void set(std::function<void(Args...)> value)
    {
        _value = value ? std::make_shared<std::function<void(Args...)>>(std::move(value)) : nullptr;
    }

    void operator()(Args... args) const
    {
        auto value = _value;
        if(value)
        {
            (*value)(std::forward<Args>(args)...);
        }
    }

private:
    std::shared_ptr<std::function<void(Args...)>> _value;
};
}

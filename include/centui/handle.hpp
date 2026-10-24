#pragma once

#include <concepts>
#include <memory>
#include <stdexcept>
#include <utility>

#include <centui/style.hpp>

namespace centui
{

class widget;
class document;
template <typename T = widget>
class handle;
template <typename T = widget>
class weak_handle;

namespace details
{

struct widget_access;

}

/// \brief Shared, typed access to a widget. Empty handles throw on dereference.
template <typename T>
class handle
{
public:
    handle() = default;

    explicit handle(std::shared_ptr<T> value)
        : m_value(std::move(value))
    {
    }

    template <typename U>
        requires std::derived_from<U, T>
    handle(const handle<U>& other)
        : m_value(other.m_value)
    {
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return static_cast<bool>(m_value);
    }

    [[nodiscard]] T& get() const
    {
        if(!m_value)
        {
            throw std::logic_error("Access through an empty widget handle");
        }

        return *m_value;
    }

    [[nodiscard]] const std::shared_ptr<T>& operator->() const
    {
        (void)get();

        return m_value;
    }

    [[nodiscard]] T& operator*() const
    {
        return get();
    }

    [[nodiscard]] const centui::style& style() const
    {
        return get().style();
    }

    void set_style(centui::style value)
    {
        get().set_style(std::move(value));
    }

    void patch_style(const style_patch& patch)
    {
        get().patch_style(patch);
    }

    [[nodiscard]] weak_handle<T> weak() const noexcept
    {
        return weak_handle<T>{*this};
    }

    [[nodiscard]] bool operator==(const handle&) const noexcept = default;

private:
    template <typename>
    friend class handle;

    template <typename>
    friend class weak_handle;
    friend class document;
    friend class widget;
    friend struct details::widget_access;

    std::shared_ptr<T> m_value;
};

/// \brief Observes a widget without extending its lifetime.
/// Use weak handles when callbacks capture their own widget or an ancestor.
template <typename T>
class weak_handle
{
public:
    weak_handle() = default;

    weak_handle(const handle<T>& value)
        : m_value(value.m_value)
    {
    }

    [[nodiscard]] handle<T> lock() const noexcept
    {
        return handle<T>{m_value.lock()};
    }

private:
    std::weak_ptr<T> m_value;
};

namespace details
{

struct widget_access
{
    static void finish(const std::shared_ptr<widget>& parent);
};

} // namespace details

template <typename T, typename... Args>
    requires std::derived_from<T, widget> && (!std::is_abstract_v<T>) && std::constructible_from<T, Args...>
[[nodiscard]] handle<T> make_widget(Args&&... args)
{
    auto value = std::make_shared<T>(std::forward<Args>(args)...);

    details::widget_access::finish(value);

    return handle<T>{std::move(value)};
}

} // namespace centui

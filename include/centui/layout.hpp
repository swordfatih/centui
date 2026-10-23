#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <centui/style.hpp>
#include <centui/types.hpp>

namespace centui
{

/// \brief Owns a move-only, const-invocable widget measurement callback.
class measure_function
{
public:
    measure_function() = default;

    measure_function(measure_function&&) noexcept = default;

    measure_function& operator=(measure_function&&) noexcept = default;

    template <typename function>
        requires(
            !std::is_same_v<std::decay_t<function>, measure_function> &&
            std::is_invocable_r_v<size, const std::decay_t<function>&, axis_constraint, axis_constraint>
        )
    measure_function(function&& value)
        : m_callable(std::make_unique<implementation<std::decay_t<function>>>(std::forward<function>(value)))
    {
    }

    explicit operator bool() const noexcept;

    size operator()(axis_constraint width, axis_constraint height) const;

private:
    struct callable
    {
        virtual ~callable() = default;

        virtual size invoke(axis_constraint width, axis_constraint height) const = 0;
    };

    template <typename function>
    struct implementation final : callable
    {
        function value;

        explicit implementation(function input)
            : value(std::move(input))
        {
        }

        size invoke(axis_constraint width, axis_constraint height) const override
        {
            return std::invoke(value, width, height);
        }
    };

    std::unique_ptr<callable> m_callable;
};

/// \brief Owns a Yoga node and its configuration; native resources are released together.
class layout
{
public:
    layout();

    ~layout();

    layout(const layout&) = delete;

    layout& operator=(const layout&) = delete;

    void update(const style& style);

    void insert_child(const layout& child);

    void calculate(float width, float height);

    [[nodiscard]] rect bounds() const;

    [[nodiscard]] rect content_bounds() const;

    void rethrow_measure_failure();

    void set_measure(measure_function measure);

private:
    struct data;
    std::unique_ptr<data> m_data;
};

} // namespace centui

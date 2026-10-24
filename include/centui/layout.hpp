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

/// \brief Document-wide Yoga behavior. Zero scale preserves fractional logical coordinates.
/// Compatibility flags opt into individual historical Yoga behaviors.
struct layout_options
{
    layout_direction root_direction = layout_direction::ltr;
    float point_scale_factor = 0;
    bool stretch_flex_basis = false;
    bool absolute_position_without_insets_excludes_padding = false;
    bool absolute_percent_against_inner_size = false;
    bool experimental_web_flex_basis = false;

    void validate() const;

    bool operator==(const layout_options&) const = default;
};

/// \brief Resolved physical edges in logical units.
struct computed_edges
{
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;

    bool operator==(const computed_edges&) const = default;
};

/// \brief Yoga results from the most recent document preparation.
struct layout_metrics
{
    computed_edges margin;
    computed_edges padding;
    computed_edges border;
    layout_direction direction = layout_direction::ltr;
    bool had_overflow = false;
};

/// \brief Returns a baseline offset from the content top for the supplied content size.
/// An empty function lets Yoga derive the baseline from children or the bottom edge.
using baseline_function = std::function<float(size)>;

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
    explicit layout(layout_options options = {});

    ~layout();

    layout(const layout&) = delete;

    layout& operator=(const layout&) = delete;

    void update(const style& style);

    void insert_child(const layout& child);

    void calculate(float width, float height);

    [[nodiscard]] rect bounds() const;

    [[nodiscard]] rect content_bounds() const;

    [[nodiscard]] layout_metrics metrics() const;

    void rethrow_measure_failure();

    void set_measure(measure_function measure);

    void set_baseline(baseline_function baseline);

private:
    struct data;
    std::unique_ptr<data> m_data;
};

} // namespace centui

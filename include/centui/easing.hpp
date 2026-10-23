#pragma once

#include <algorithm>
#include <cmath>

namespace centui::easing
{

/// \brief Available easing curves for normalized animation progress.
enum class curve
{
    linear,
    in_quad,
    out_quad,
    in_out_quad,
    in_cubic,
    out_cubic,
    in_out_cubic,
    out_back
};

[[nodiscard]] constexpr float sample(curve shape, float progress) noexcept
{
    const auto t = std::clamp(progress, 0.0f, 1.0f);

    switch(shape)
    {
    case curve::in_quad:
    {
        return t * t;
    }

    case curve::out_quad:
    {
        return t * (2 - t);
    }

    case curve::in_out_quad:
    {
        return t < 0.5f ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t);
    }

    case curve::in_cubic:
    {
        return t * t * t;
    }

    case curve::out_cubic:
    {
        return 1 - (1 - t) * (1 - t) * (1 - t);
    }

    case curve::in_out_cubic:
    {
        return t < 0.5f ? 4 * t * t * t : 1 - 4 * (1 - t) * (1 - t) * (1 - t);
    }

    case curve::out_back:
    {
        const auto x = t - 1;

        return 1 + 2.70158f * x * x * x + 1.70158f * x * x;
    }

    default:
    {
        return t;
    }
    }
}

} // namespace centui::easing

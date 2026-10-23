#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <stdexcept>

#include <centui/easing.hpp>

namespace centui
{

/// \brief Interpolates a floating-point value along an easing curve.
template <std::floating_point T = float>
class animation
{
public:
    using duration = std::chrono::duration<float>;

    animation(T from, T to, duration length, easing::curve curve = easing::curve::linear)
    {
        reset(from, to, length, curve);
    }

    void reset(T from, T to, duration length, easing::curve curve = easing::curve::linear)
    {
        if(!std::isfinite(from) || !std::isfinite(to) || !std::isfinite(length.count()) || length < duration::zero())
        {
            throw std::invalid_argument("Animation values must be finite and duration nonnegative");
        }

        m_from = from;
        m_to = to;
        m_length = length;

        m_elapsed = duration::zero();

        m_curve = curve;
    }

    void advance(duration elapsed)
    {
        if(!std::isfinite(elapsed.count()) || elapsed < duration::zero())
        {
            throw std::invalid_argument("Elapsed time must be finite and nonnegative");
        }

        m_elapsed += std::min(elapsed, m_length - m_elapsed);
    }

    [[nodiscard]] bool finished() const noexcept
    {
        return m_elapsed >= m_length;
    }

    [[nodiscard]] T value() const noexcept
    {
        if(finished())
        {
            return m_to;
        }

        return std::lerp(m_from, m_to, static_cast<T>(easing::sample(m_curve, m_elapsed / m_length)));
    }

private:
    T m_from = 0;
    T m_to = 0;
    duration m_length{};
    duration m_elapsed{};
    easing::curve m_curve = easing::curve::linear;
};

} // namespace centui

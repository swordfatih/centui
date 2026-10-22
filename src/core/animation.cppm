module;

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <stdexcept>

export module centui.animation;
export import centui.easing;

export namespace centui
{
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

        _from = from;
        _to = to;
        _length = length;
        _elapsed = duration::zero();
        _curve = curve;
    }

    void advance(duration elapsed)
    {
        if(!std::isfinite(elapsed.count()) || elapsed < duration::zero())
        {
            throw std::invalid_argument("Elapsed time must be finite and nonnegative");
        }

        _elapsed += std::min(elapsed, _length - _elapsed);
    }

    [[nodiscard]] bool finished() const noexcept
    {
        return _elapsed >= _length;
    }

    [[nodiscard]] T value() const noexcept
    {
        if(finished())
        {
            return _to;
        }

        return std::lerp(_from, _to, static_cast<T>(easing::sample(_curve, _elapsed / _length)));
    }

private:
    T _from = 0;
    T _to = 0;
    duration _length{};
    duration _elapsed{};
    easing::curve _curve = easing::curve::linear;
};
}

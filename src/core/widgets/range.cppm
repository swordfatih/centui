module;

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <utility>

export module centui.widgets.range;
export import centui.tree;
import centui.widgets.callback;

export namespace centui
{
class range_widget : public widget
{
public:
    explicit range_widget(centui::style appearance = {}, float minimum = 0, float maximum = 1, float step = 0.01f)
        : widget(std::move(appearance))
    {
        set_range(minimum, maximum, step);
        set_interactive();
        set_focusable();
    }

    void set_range(float minimum, float maximum, float step)
    {
        check_mutation();
        if(!std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(step) ||
           maximum <= minimum || !std::isfinite(maximum - minimum) || step <= 0)
        {
            throw std::invalid_argument("Range requires finite increasing bounds and a positive step");
        }

        _minimum = minimum;
        _maximum = maximum;
        _step = step;
        set_value(_value);
        invalidate_paint();
    }

    void set_value(float value)
    {
        check_mutation();
        if(!std::isfinite(value))
        {
            throw std::invalid_argument("Range value must be finite");
        }

        const auto bounded = std::clamp(value, _minimum, _maximum);
        const auto steps = std::round((static_cast<double>(bounded) - _minimum) / _step);
        _value = static_cast<float>(std::clamp(_minimum + steps * _step, static_cast<double>(_minimum), static_cast<double>(_maximum)));
        if(bounded == _maximum)
        {
            _value = _maximum;
        }
        invalidate_paint();
    }

    [[nodiscard]] float value() const noexcept
    {
        return _value;
    }

    void set_on_change(std::function<void(float)> callback)
    {
        check_mutation();
        _on_change.set(std::move(callback));
    }

protected:
    size measure(const measure_context&) override
    {
        return {160, 24};
    }

    void paint(painter& output) const override
    {
        const auto box = output.bounds();
        const auto thumb = std::min(12.0f, box.width);
        const auto x = box.x + (box.width - thumb) * (_value - _minimum) / (_maximum - _minimum);
        output.rectangle({box.x, box.y + box.height / 2 - 1, box.width, 2}, output.foreground());
        output.rectangle({x, box.y, thumb, box.height}, output.foreground());
    }

    bool on_event(const input_event& event) override
    {
        if(event.type == input_event_type::pointer_cancelled)
        {
            _dragging = false;
            return true;
        }

        if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
        {
            _dragging = true;
        }

        if(_dragging && (event.type == input_event_type::pointer_pressed || event.type == input_event_type::pointer_moved ||
                         event.type == input_event_type::pointer_released))
        {
            const auto box = content_bounds();
            const auto thumb = std::min(12.0f, box.width);
            const auto fraction = box.width > thumb ? std::clamp((event.x - box.x - thumb / 2) / (box.width - thumb), 0.0f, 1.0f) : 0.0f;
            change(std::lerp(_minimum, _maximum, fraction));
            if(event.type == input_event_type::pointer_released)
            {
                _dragging = false;
            }

            return true;
        }

        if(event.type != input_event_type::key_pressed)
        {
            return false;
        }

        switch(event.code)
        {
        case key::left:
        case key::down:
        {
            change(std::max(_minimum, _value - _step));
            return true;
        }
        case key::right:
        case key::up:
        {
            change(std::min(_maximum, _value + _step));
            return true;
        }
        case key::home:
        {
            change(_minimum);
            return true;
        }
        case key::end:
        {
            change(_maximum);
            return true;
        }
        default:
        {
            return false;
        }
        }
    }

private:
    void change(float value)
    {
        const auto previous = _value;
        set_value(value);
        if(previous != _value)
        {
            _on_change(_value);
        }
    }

    float _minimum = 0;
    float _maximum = 1;
    float _step = 0.01f;
    float _value = 0;
    bool _dragging = false;
    details::callback<float> _on_change;
};

struct range_options
{
    float minimum = 0;
    float maximum = 1;
    float step = 0.01f;
    float value = 0;
    std::function<void(float)> on_change{};
};

template <typename... Styles>
class range : public handle<range_widget>
{
public:
    explicit range(range_options options = {}, centui::style appearance = centui::style::resolve<Styles...>())
        : handle<range_widget>(make_widget<range_widget>(std::move(appearance), options.minimum, options.maximum, options.step))
    {
        this->get().set_value(options.value);
        this->get().set_on_change(std::move(options.on_change));
    }
};
}

module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module centui.controls;
export import centui.widgets;

export namespace centui
{
class toggle_widget : public text_button_widget
{
public:
    explicit toggle_widget(std::string label, centui::style appearance = {}, bool checked = false)
        : text_button_widget(std::move(label), std::move(appearance)), _checked(checked)
    {
    }

    [[nodiscard]] bool checked() const noexcept
    {
        return _checked;
    }

    void set_checked(bool value)
    {
        check_mutation();
        if(_checked != value)
        {
            _checked = value;
            invalidate_paint();
        }
    }

    void set_on_change(std::function<void(bool)> callback)
    {
        check_mutation();
        _on_change.set(std::move(callback));
    }

protected:
    void activate() override
    {
        set_checked(!_checked);
        _on_change(_checked);
        button_widget::activate();
    }

    void paint(painter& output) const override
    {
        text_button_widget::paint(output);
        if(_checked)
        {
            auto line = output.bounds();
            line.y += std::max(0.0f, line.height - 2);
            line.height = std::min(2.0f, line.height);
            output.rectangle(line, output.foreground());
        }
    }

private:
    bool _checked = false;
    details::callback<bool> _on_change;
};

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

class scroll_widget : public container_widget
{
public:
    explicit scroll_widget(centui::style appearance = {}) : container_widget(std::move(appearance))
    {
        patch_style({.overflow = overflow::scroll});
        set_interactive();
        set_focusable();
    }

    [[nodiscard]] vec2 offset() const noexcept
    {
        return _offset;
    }

    [[nodiscard]] vec2 limit() const noexcept
    {
        return _limit;
    }

    void set_offset(vec2 value)
    {
        check_mutation();
        if(!std::isfinite(value.x) || !std::isfinite(value.y))
        {
            throw std::invalid_argument("Scroll offset must be finite");
        }

        _offset = {std::clamp(value.x, 0.0f, _limit.x), std::clamp(value.y, 0.0f, _limit.y)};
        set_child_offset({-_offset.x, -_offset.y});
    }

protected:
    void on_layout() override
    {
        const auto area = content_bounds();
        vec2 extent{};
        for(const auto& child: children())
        {
            if(child->appearance().visibility == visibility::collapsed)
            {
                continue;
            }

            const auto box = child->bounds();
            extent.x = std::max(extent.x, box.x + box.width + _offset.x - area.x);
            extent.y = std::max(extent.y, box.y + box.height + _offset.y - area.y);
        }

        _limit = {std::max(0.0f, extent.x - area.width), std::max(0.0f, extent.y - area.height)};
        set_offset(_offset);
    }

    bool on_event(const input_event& event) override
    {
        auto next = _offset;
        if(event.type == input_event_type::wheel)
        {
            next.x += event.delta_x;
            next.y += event.delta_y;
        }
        else if(event.type == input_event_type::key_pressed && event.code == key::page_down)
        {
            next.y += content_bounds().height;
        }
        else if(event.type == input_event_type::key_pressed && event.code == key::page_up)
        {
            next.y -= content_bounds().height;
        }
        else
        {
            return false;
        }

        const auto previous = _offset;
        set_offset(next);
        return _offset != previous;
    }

private:
    vec2 _offset{};
    vec2 _limit{};
};

class selection_widget : public widget
{
public:
    explicit selection_widget(std::vector<std::string> items, centui::style appearance = {})
        : widget(std::move(appearance)), _items(std::move(items))
    {
        set_interactive();
        set_focusable();
    }

    void set_items(std::vector<std::string> items)
    {
        check_mutation();
        _items = std::move(items);
        _selected.reset();
        invalidate_layout();
    }

    void set_selected(std::optional<std::size_t> index)
    {
        check_mutation();
        if(index && *index >= _items.size())
        {
            throw std::out_of_range("Selection is outside the item list");
        }

        _selected = index;
        invalidate_paint();
    }

    [[nodiscard]] std::optional<std::size_t> selected() const noexcept
    {
        return _selected;
    }

    void set_on_change(std::function<void(std::size_t)> callback)
    {
        check_mutation();
        _on_change.set(std::move(callback));
    }

protected:
    size measure(const measure_context& context) override
    {
        const auto appearance = resolved_style();
        float width = 0;
        for(const auto& item: _items)
        {
            width = std::max(width, context.text.prepare(item, appearance.font, appearance.font_size)->bounds.width);
        }

        return {width, row_height() * _items.size()};
    }

    void paint(painter& output) const override
    {
        const auto appearance = resolved_style();
        auto row = output.bounds();
        row.height = row_height();
        for(std::size_t index = 0; index < _items.size(); ++index)
        {
            if(_selected == index)
            {
                auto tint = output.foreground();
                tint.a = static_cast<std::uint8_t>(tint.a / 4);
                output.rectangle(row, tint);
            }

            output.text(output.fonts().prepare(_items[index], appearance.font, appearance.font_size, row.width), row, output.foreground());
            row.y += row.height;
        }
    }

    bool on_event(const input_event& event) override
    {
        if(_items.empty())
        {
            return false;
        }

        std::optional<std::size_t> next;
        if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
        {
            const auto row = std::floor((event.y - content_bounds().y) / row_height());
            if(row >= 0 && row < _items.size())
            {
                next = static_cast<std::size_t>(row);
            }
        }
        else if(event.type == input_event_type::key_pressed)
        {
            switch(event.code)
            {
            case key::home:
            {
                next = 0;
                break;
            }
            case key::end:
            {
                next = _items.size() - 1;
                break;
            }
            case key::down:
            {
                next = _selected ? std::min(*_selected + 1, _items.size() - 1) : 0;
                break;
            }
            case key::up:
            {
                next = _selected ? (*_selected == 0 ? 0 : *_selected - 1) : _items.size() - 1;
                break;
            }
            default:
            {
                break;
            }
            }
        }

        if(!next)
        {
            return false;
        }

        if(_selected != next)
        {
            set_selected(next);
            _on_change(*next);
        }

        return true;
    }

private:
    float row_height() const noexcept
    {
        return style().font_size * 1.6f;
    }

    std::vector<std::string> _items;
    std::optional<std::size_t> _selected;
    details::callback<std::size_t> _on_change;
};

class popup_widget : public container_widget
{
public:
    explicit popup_widget(handle<> content, centui::style appearance = {}) : container_widget(std::move(appearance))
    {
        patch_style({.inset = edges::all(px(0)), .position = position::absolute,
                     .width = percent(100), .height = percent(100), .visibility = visibility::collapsed});
        append(content);
        set_interactive();
        set_focusable();
        set_focus_scope();
    }

    void open()
    {
        check_mutation();
        if(style().visibility == visibility::visible)
        {
            return;
        }

        auto root = parent();
        while(root.valid() && root->parent().valid())
        {
            root = root->parent();
        }

        if(root.valid())
        {
            _previous_focus = find_focus(root).weak();
        }

        _focus_pending = true;
        patch_style({.visibility = visibility::visible});
    }

    void close()
    {
        check_mutation();
        if(style().visibility != visibility::visible)
        {
            return;
        }

        _focus_pending = false;
        patch_style({.visibility = visibility::collapsed});
        if(auto previous = _previous_focus.lock(); previous.valid())
        {
            previous->request_focus();
        }

        _on_close();
    }

    void set_on_close(std::function<void()> callback)
    {
        check_mutation();
        _on_close.set(std::move(callback));
    }

protected:
    void on_layout() override
    {
        if(_focus_pending && effectively_visible())
        {
            _focus_pending = false;
            focus_next();
        }
    }

    bool on_event(const input_event& event) override
    {
        if(event.type == input_event_type::key_pressed && event.code == key::escape)
        {
            close();
            return true;
        }

        if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
        {
            for(const auto& child: children())
            {
                const auto box = child->bounds();
                if(event.x >= box.x && event.y >= box.y && event.x < box.x + box.width && event.y < box.y + box.height)
                {
                    return true;
                }
            }

            close();
            return true;
        }

        return event.type == input_event_type::wheel;
    }

private:
    static handle<> find_focus(const handle<>& root)
    {
        if(root->focused())
        {
            return root;
        }

        for(const auto& child: root->children())
        {
            if(auto result = find_focus(child); result.valid())
            {
                return result;
            }
        }

        return {};
    }

    bool _focus_pending = false;
    weak_handle<> _previous_focus;
    details::callback<> _on_close;
};
}

export namespace centui
{
struct toggle_options
{
    bool checked = false;
    std::function<void(bool)> on_change{};
};

struct range_options
{
    float minimum = 0;
    float maximum = 1;
    float step = 0.01f;
    float value = 0;
    std::function<void(float)> on_change{};
};

struct selection_options
{
    std::optional<std::size_t> selected{};
    std::function<void(std::size_t)> on_change{};
};

template <typename... Styles>
class toggle : public handle<toggle_widget>
{
public:
    explicit toggle(std::string label, toggle_options options = {},
                    centui::style appearance = centui::style::resolve<Styles...>())
        : handle<toggle_widget>(make_widget<toggle_widget>(std::move(label), std::move(appearance), options.checked))
    {
        this->get().set_on_change(std::move(options.on_change));
    }
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

template <typename... Styles>
class selection : public handle<selection_widget>
{
public:
    explicit selection(std::vector<std::string> items, selection_options options = {},
                       centui::style appearance = centui::style::resolve<Styles...>())
        : handle<selection_widget>(make_widget<selection_widget>(std::move(items), std::move(appearance)))
    {
        this->get().set_selected(options.selected);
        this->get().set_on_change(std::move(options.on_change));
    }
};

template <typename... Styles>
class scroll : public handle<scroll_widget>
{
public:
    scroll() : scroll(std::initializer_list<handle<>>{})
    {
    }

    explicit scroll(std::initializer_list<handle<>> children,
                    centui::style appearance = centui::style::resolve<Styles...>())
        : handle<scroll_widget>(make_widget<scroll_widget>(std::move(appearance)))
    {
        this->get().append(std::span{children.begin(), children.size()});
    }
};

template <typename... Styles>
class popup : public handle<popup_widget>
{
public:
    explicit popup(handle<> content, centui::style appearance = centui::style::resolve<Styles...>())
        : handle<popup_widget>(make_widget<popup_widget>(std::move(content), std::move(appearance)))
    {
    }
};
}

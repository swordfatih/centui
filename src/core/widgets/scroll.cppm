module;

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <utility>

export module centui.widgets.scroll;
export import centui.widgets.container;

export namespace centui
{
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
}

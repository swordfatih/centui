module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <memory>

export module centui.widgets.selection;
export import centui.tree;
import centui.widgets.callback;

export namespace centui
{
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

struct selection_options
{
    std::optional<std::size_t> selected{};
    std::function<void(std::size_t)> on_change{};
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
}

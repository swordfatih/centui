module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module centui.input;
export import centui.widgets;

export namespace centui
{
class input_widget : public widget
{
public:
    explicit input_widget(std::string value = {}, centui::style appearance = {}, bool multiline = false)
        : widget(std::move(appearance)), _multiline(multiline)
    {
        set_text(std::move(value));
        set_interactive();
        set_focusable();
    }

    [[nodiscard]] std::string_view value() const noexcept
    {
        return _value;
    }

    void set_text(std::string value)
    {
        check_mutation();
        _value = normalize(std::move(value));
        _anchor = _caret = _value.size();
        _preedit.clear();
        _undo.clear();
        _redo.clear();
        changed_layout();
    }

    void set_selection(std::size_t anchor, std::size_t caret)
    {
        check_mutation();
        _anchor = boundary(std::min(anchor, _value.size()));
        _caret = boundary(std::min(caret, _value.size()));
        if(!_preedit.empty())
        {
            _layout.reset();
        }
        _preedit.clear();
        _reveal_caret = true;
        invalidate_layout();
    }

    [[nodiscard]] std::pair<std::size_t, std::size_t> selection() const noexcept
    {
        return std::minmax(_anchor, _caret);
    }

    void set_on_change(std::function<void(std::string_view)> callback)
    {
        check_mutation();
        _on_change.set(std::move(callback));
    }

    void set_on_submit(std::function<void()> callback)
    {
        check_mutation();
        _on_submit.set(std::move(callback));
    }

protected:
    size measure(const measure_context& context) override
    {
        const auto width = _multiline && context.width.mode != constraint_mode::unconstrained ?
            context.width.value : std::numeric_limits<float>::infinity();
        prepare_text(context.text, width);
        return {std::max(160.0f, _layout->bounds.width), std::max(style().font_size * 1.5f, _layout->bounds.height)};
    }

    void on_layout() override
    {
        const auto area = content_bounds();
        prepare_text(fonts(), _multiline ? area.width : std::numeric_limits<float>::infinity());
        const auto caret = caret_bounds(*_layout, display_caret());
        if(_reveal_caret)
        {
            _scroll.x = std::max(caret.x + caret.width - area.width, std::min(_scroll.x, caret.x));
            _scroll.y = std::max(caret.y + caret.height - area.height, std::min(_scroll.y, caret.y));
            _reveal_caret = false;
        }

        _scroll.x = std::clamp(_scroll.x, 0.0f, std::max(0.0f, _layout->bounds.width + 1 - area.width));
        _scroll.y = std::clamp(_scroll.y, 0.0f, std::max(0.0f, _layout->bounds.height - area.height));
    }

    void paint(painter& output) const override
    {
        if(!_layout)
        {
            return;
        }

        auto area = output.bounds();
        area.x -= _scroll.x;
        area.y -= _scroll.y;
        area.width = std::max(area.width, _layout->bounds.width);
        area.height = std::max(area.height, _layout->bounds.height);
        const auto [begin, end] = selection();
        auto highlight = output.foreground();
        highlight.a = static_cast<std::uint8_t>(highlight.a / 3);

        if(_preedit.empty())
        {
            for(auto box: selection_bounds(*_layout, begin, end))
            {
                box.x += area.x;
                box.y += area.y;
                output.rectangle(box, highlight);
            }
        }
        else
        {
            for(auto box: selection_bounds(*_layout, begin, begin + _preedit.size()))
            {
                box.x += area.x;
                box.y += area.y + box.height - 1;
                box.height = 1;
                output.rectangle(box, output.foreground());
            }
        }

        output.text(_layout, area, output.foreground());
        if(focused())
        {
            auto caret = caret_bounds(*_layout, display_caret());
            caret.x += area.x;
            caret.y += area.y;
            output.rectangle(caret, output.foreground());
        }
    }

    std::optional<rect> text_input_area() const override
    {
        if(!_layout)
        {
            return content_bounds();
        }

        auto caret = caret_bounds(*_layout, display_caret());
        caret.x += content_bounds().x - _scroll.x;
        caret.y += content_bounds().y - _scroll.y;
        return caret;
    }

    void on_focus_changed(bool value) override
    {
        if(!value)
        {
            _dragging = false;
            _preedit.clear();
            changed_layout();
        }
    }

    bool on_event(const input_event& event) override
    {
        if(event.type == input_event_type::composition)
        {
            _preedit = normalize(event.text);
            _preedit_caret = std::min(event.selection_start, _preedit.size());
            changed_layout();
            return true;
        }

        if(event.type == input_event_type::text_entered)
        {
            replace_selection(normalize(event.text));
            return true;
        }

        if(event.type == input_event_type::pointer_cancelled)
        {
            _dragging = false;
            return true;
        }

        if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
        {
            _preedit.clear();
            _layout.reset();
            on_layout();
            _dragging = true;
            move_caret(hit(event.x, event.y), event.mods.shift);
            return true;
        }

        if(_dragging && (event.type == input_event_type::pointer_moved || event.type == input_event_type::pointer_released))
        {
            move_caret(hit(event.x, event.y), true);
            if(event.type == input_event_type::pointer_released)
            {
                _dragging = false;
            }

            return true;
        }

        if(event.type == input_event_type::wheel && _layout)
        {
            const auto previous = _scroll;
            _scroll.x = std::clamp(_scroll.x + event.delta_x, 0.0f, std::max(0.0f, _layout->bounds.width + 1 - content_bounds().width));
            _scroll.y = std::clamp(_scroll.y + event.delta_y, 0.0f, std::max(0.0f, _layout->bounds.height - content_bounds().height));
            invalidate_paint();
            return previous != _scroll;
        }

        if(event.type != input_event_type::key_pressed)
        {
            return false;
        }

        if(event.code == key::escape && !_preedit.empty())
        {
            _preedit.clear();
            changed_layout();
            return true;
        }

        if(!_preedit.empty())
        {
            return event.code != key::tab;
        }

        const auto [begin, end] = selection();
        if(event.mods.control || event.mods.system)
        {
            if(event.code == key::a)
            {
                set_selection(0, _value.size());
                return true;
            }

            if(event.code == key::c || event.code == key::x)
            {
                if(begin != end && host().write_clipboard)
                {
                    host().write_clipboard(std::string_view{_value}.substr(begin, end - begin));
                    if(event.code == key::x)
                    {
                        replace_selection({});
                    }
                }

                return true;
            }

            if(event.code == key::v)
            {
                if(host().read_clipboard)
                {
                    replace_selection(normalize(host().read_clipboard()));
                }

                return true;
            }

            if(event.code == key::z || event.code == key::y)
            {
                const bool redo = event.code == key::y || event.mods.shift;
                restore(redo ? _redo : _undo, redo ? _undo : _redo);
                return true;
            }
        }

        switch(event.code)
        {
        case key::left:
        {
            move_caret(!event.mods.shift && begin != end ? begin : adjacent(false), event.mods.shift);
            return true;
        }
        case key::right:
        {
            move_caret(!event.mods.shift && begin != end ? end : adjacent(true), event.mods.shift);
            return true;
        }
        case key::up:
        case key::down:
        {
            if(_multiline && _layout)
            {
                const auto caret = caret_bounds(*_layout, _caret);
                move_caret(text_hit(*_layout, {caret.x, caret.y + caret.height * (event.code == key::up ? -0.5f : 1.5f)}), event.mods.shift);
            }

            return _multiline;
        }
        case key::home:
        case key::end:
        {
            auto next = event.code == key::home ? std::size_t{0} : _value.size();
            if(_layout && !event.mods.control && !event.mods.system)
            {
                const auto caret = caret_bounds(*_layout, _caret);
                next = text_hit(*_layout, {event.code == key::home ? -1.0f : _layout->bounds.width + 1, caret.y + caret.height / 2});
            }

            move_caret(next, event.mods.shift);
            return true;
        }
        case key::backspace:
        case key::delete_:
        {
            if(begin == end)
            {
                _anchor = adjacent(event.code == key::delete_);
            }

            replace_selection({});
            return true;
        }
        case key::enter:
        {
            if(_multiline)
            {
                replace_selection("\n");
            }
            else if(!event.repeat)
            {
                _on_submit();
            }

            return true;
        }
        default:
        {
            return false;
        }
        }
    }

private:
    struct snapshot
    {
        std::string value;
        std::size_t anchor;
        std::size_t caret;
    };

    std::string normalize(std::string value) const
    {
        std::string result;
        for(std::size_t index = 0; index < value.size();)
        {
            const auto first = static_cast<unsigned char>(value[index]);
            const std::size_t count = first < 0x80 ? 1 : first >= 0xC2 && first <= 0xDF ? 2 :
                                      first >= 0xE0 && first <= 0xEF ? 3 : first >= 0xF0 && first <= 0xF4 ? 4 : 0;
            if(count == 0 || index + count > value.size())
            {
                throw std::invalid_argument("Input requires valid UTF-8");
            }

            std::uint32_t scalar = first & (count == 1 ? 0x7F : count == 2 ? 0x1F : count == 3 ? 0x0F : 0x07);
            for(std::size_t byte = 1; byte < count; ++byte)
            {
                const auto continuation = static_cast<unsigned char>(value[index + byte]);
                if((continuation & 0xC0) != 0x80)
                {
                    throw std::invalid_argument("Input requires valid UTF-8");
                }

                scalar = (scalar << 6) | (continuation & 0x3F);
            }

            if((count == 2 && scalar < 0x80) || (count == 3 && scalar < 0x800) ||
               (count == 4 && scalar < 0x10000) || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
            {
                throw std::invalid_argument("Input requires valid UTF-8");
            }

            if(first == '\r' || first == '\n')
            {
                result += _multiline ? '\n' : ' ';
                if(first == '\r' && index + 1 < value.size() && value[index + 1] == '\n')
                {
                    ++index;
                }
            }
            else if(first == '\t')
            {
                result += ' ';
            }
            else if(scalar >= 32 && scalar != 127)
            {
                result.append(value, index, count);
            }

            index += count;
        }

        return result;
    }

    std::size_t boundary(std::size_t offset) const noexcept
    {
        while(offset > 0 && offset < _value.size() && (static_cast<unsigned char>(_value[offset]) & 0xC0) == 0x80)
        {
            --offset;
        }

        return offset;
    }

    std::size_t adjacent(bool forward) const
    {
        auto result = forward ? _value.size() : 0;
        if(_layout)
        {
            for(const auto& caret: _layout->carets)
            {
                if(forward && caret.offset > _caret)
                {
                    result = std::min(result, caret.offset);
                }
                else if(!forward && caret.offset < _caret)
                {
                    result = std::max(result, caret.offset);
                }
            }
        }

        return result;
    }

    std::size_t hit(float x, float y) const
    {
        return _layout ? text_hit(*_layout, {x - content_bounds().x + _scroll.x, y - content_bounds().y + _scroll.y}) : 0;
    }

    void move_caret(std::size_t next, bool extend)
    {
        set_selection(extend ? _anchor : next, next);
    }

    std::size_t display_caret() const noexcept
    {
        return _preedit.empty() ? _caret : std::min(_anchor, _caret) + _preedit_caret;
    }

    void prepare_text(text_service& service, float width) const
    {
        const auto appearance = resolved_style();
        const auto identity = service.font_identity(appearance.font);
        if(_layout && _font_identity == identity && _font_size == style().font_size && _width == width &&
           _layout->raster_scale == service.scale())
        {
            return;
        }

        auto text = _value;
        if(!_preedit.empty())
        {
            const auto [begin, end] = selection();
            text.replace(begin, end - begin, _preedit);
        }

        _layout = service.prepare(text, appearance.font, appearance.font_size, width);
        _font_identity = identity;
        _font_size = style().font_size;
        _width = width;
    }

    void changed_layout()
    {
        _layout.reset();
        _reveal_caret = true;
        invalidate_layout();
    }

    void notify_change()
    {
        const auto value = _value;
        _on_change(value);
    }

    void replace_selection(std::string replacement)
    {
        const auto [begin, end] = selection();
        _preedit.clear();
        if(begin == end && replacement.empty())
        {
            changed_layout();
            return;
        }

        auto candidate = _value;
        candidate.replace(begin, end - begin, replacement);
        _undo.push_back({_value, _anchor, _caret});
        if(_undo.size() > 128)
        {
            _undo.erase(_undo.begin());
        }

        _redo.clear();
        _value = std::move(candidate);
        _anchor = _caret = begin + replacement.size();
        changed_layout();
        notify_change();
    }

    void restore(std::vector<snapshot>& source, std::vector<snapshot>& destination)
    {
        if(source.empty())
        {
            return;
        }

        destination.push_back({_value, _anchor, _caret});
        auto state = std::move(source.back());
        source.pop_back();
        _value = std::move(state.value);
        _anchor = state.anchor;
        _caret = state.caret;
        changed_layout();
        notify_change();
    }

    std::string _value;
    std::string _preedit;
    std::size_t _anchor = 0;
    std::size_t _caret = 0;
    std::size_t _preedit_caret = 0;
    bool _multiline = false;
    bool _dragging = false;
    bool _reveal_caret = true;
    vec2 _scroll{};
    std::vector<snapshot> _undo;
    std::vector<snapshot> _redo;
    details::callback<std::string_view> _on_change;
    details::callback<> _on_submit;
    mutable std::shared_ptr<const text_layout> _layout;
    mutable std::shared_ptr<const void> _font_identity;
    mutable float _font_size = 0;
    mutable float _width = 0;
};
}

export namespace centui
{
struct input_options
{
    bool multiline = false;
    std::function<void(std::string_view)> on_change{};
    std::function<void()> on_submit{};
};

template <typename... Styles>
class input : public handle<input_widget>
{
public:
    explicit input(std::string value = {}, input_options options = {},
                   centui::style appearance = centui::style::resolve<Styles...>())
        : handle<input_widget>(make_widget<input_widget>(std::move(value), std::move(appearance), options.multiline))
    {
        this->get().set_on_change(std::move(options.on_change));
        this->get().set_on_submit(std::move(options.on_submit));
    }
};
}

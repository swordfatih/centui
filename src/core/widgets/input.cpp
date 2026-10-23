#include <centui/widgets/input.hpp>

namespace centui
{

input_widget::input_widget(std::string value, centui::style appearance, bool multiline)
    : widget(std::move(appearance)),
      m_multiline(multiline)
{
    set_text(std::move(value));
    set_interactive();
    set_focusable();
}

std::string_view input_widget::value() const noexcept
{
    return m_value;
}

void input_widget::set_text(std::string value)
{
    check_mutation();
    m_value = normalize(std::move(value));
    m_anchor = m_caret = m_value.size();
    m_preedit.clear();
    m_undo.clear();
    m_redo.clear();
    changed_layout();
}

void input_widget::set_selection(std::size_t anchor, std::size_t caret)
{
    check_mutation();
    m_anchor = boundary(std::min(anchor, m_value.size()));
    m_caret = boundary(std::min(caret, m_value.size()));

    if(!m_preedit.empty())
    {
        m_layout.reset();
    }

    m_preedit.clear();
    m_reveal_caret = true;
    invalidate_layout();
}

std::pair<std::size_t, std::size_t> input_widget::selection() const noexcept
{
    return std::minmax(m_anchor, m_caret);
}

void input_widget::set_on_change(std::function<void(std::string_view)> callback)
{
    check_mutation();
    m_on_change.set(std::move(callback));
}

void input_widget::set_on_submit(std::function<void()> callback)
{
    check_mutation();
    m_on_submit.set(std::move(callback));
}

size input_widget::measure(const measure_context& context)
{
    const auto width = m_multiline && context.width.mode != constraint_mode::unconstrained
                           ? context.width.value
                           : std::numeric_limits<float>::infinity();
    prepare_text(context.text, width);

    return {std::max(160.0f, m_layout->bounds.width), std::max(style().font_size * 1.5f, m_layout->bounds.height)};
}

void input_widget::on_layout()
{
    const auto area = content_bounds();
    prepare_text(fonts(), m_multiline ? area.width : std::numeric_limits<float>::infinity());
    const auto caret = caret_bounds(*m_layout, display_caret());

    if(m_reveal_caret)
    {
        m_scroll.x = std::max(caret.x + caret.width - area.width, std::min(m_scroll.x, caret.x));
        m_scroll.y = std::max(caret.y + caret.height - area.height, std::min(m_scroll.y, caret.y));
        m_reveal_caret = false;
    }

    m_scroll.x = std::clamp(m_scroll.x, 0.0f, std::max(0.0f, m_layout->bounds.width + 1 - area.width));
    m_scroll.y = std::clamp(m_scroll.y, 0.0f, std::max(0.0f, m_layout->bounds.height - area.height));
}

void input_widget::paint(painter& output) const
{
    if(!m_layout)
    {
        return;
    }

    auto area = output.bounds();
    area.x -= m_scroll.x;
    area.y -= m_scroll.y;
    area.width = std::max(area.width, m_layout->bounds.width);
    area.height = std::max(area.height, m_layout->bounds.height);
    const auto [begin, end] = selection();
    auto highlight = output.foreground();
    highlight.a = static_cast<std::uint8_t>(highlight.a / 3);

    if(m_preedit.empty())
    {
        for(auto box: selection_bounds(*m_layout, begin, end))
        {
            box.x += area.x;
            box.y += area.y;
            output.rectangle(box, highlight);
        }
    }
    else
    {
        for(auto box: selection_bounds(*m_layout, begin, begin + m_preedit.size()))
        {
            box.x += area.x;
            box.y += area.y + box.height - 1;
            box.height = 1;
            output.rectangle(box, output.foreground());
        }
    }

    output.text(m_layout, area, output.foreground());

    if(focused())
    {
        auto caret = caret_bounds(*m_layout, display_caret());
        caret.x += area.x;
        caret.y += area.y;
        output.rectangle(caret, output.foreground());
    }
}

std::optional<rect> input_widget::text_input_area() const
{
    if(!m_layout)
    {
        return content_bounds();
    }

    auto caret = caret_bounds(*m_layout, display_caret());
    caret.x += content_bounds().x - m_scroll.x;
    caret.y += content_bounds().y - m_scroll.y;

    return caret;
}

void input_widget::on_focus_changed(bool value)
{
    if(!value)
    {
        m_dragging = false;
        m_preedit.clear();
        changed_layout();
    }
}

bool input_widget::on_event(const input_event& event)
{
    if(event.type == input_event_type::composition)
    {
        m_preedit = normalize(event.text);
        m_preedit_caret = std::min(event.selection_start, m_preedit.size());
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
        m_dragging = false;

        return true;
    }

    if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
    {
        m_preedit.clear();
        m_layout.reset();
        on_layout();
        m_dragging = true;
        move_caret(hit(event.x, event.y), event.mods.shift);

        return true;
    }

    if(m_dragging &&
       (event.type == input_event_type::pointer_moved || event.type == input_event_type::pointer_released))
    {
        move_caret(hit(event.x, event.y), true);

        if(event.type == input_event_type::pointer_released)
        {
            m_dragging = false;
        }

        return true;
    }

    if(event.type == input_event_type::wheel && m_layout)
    {
        const auto previous = m_scroll;
        m_scroll.x = std::clamp(
            m_scroll.x + event.delta_x,
            0.0f,
            std::max(0.0f, m_layout->bounds.width + 1 - content_bounds().width)
        );
        m_scroll.y = std::clamp(
            m_scroll.y + event.delta_y,
            0.0f,
            std::max(0.0f, m_layout->bounds.height - content_bounds().height)
        );
        invalidate_paint();

        return previous != m_scroll;
    }

    if(event.type != input_event_type::key_pressed)
    {
        return false;
    }

    if(event.code == key::escape && !m_preedit.empty())
    {
        m_preedit.clear();
        changed_layout();

        return true;
    }

    if(!m_preedit.empty())
    {
        return event.code != key::tab;
    }

    const auto [begin, end] = selection();

    if(event.mods.control || event.mods.system)
    {
        if(event.code == key::a)
        {
            set_selection(0, m_value.size());

            return true;
        }

        if(event.code == key::c || event.code == key::x)
        {
            if(begin != end && host().write_clipboard)
            {
                host().write_clipboard(std::string_view{m_value}.substr(begin, end - begin));

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
            restore(redo ? m_redo : m_undo, redo ? m_undo : m_redo);

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
        if(m_multiline && m_layout)
        {
            const auto caret = caret_bounds(*m_layout, m_caret);
            move_caret(
                text_hit(*m_layout, {caret.x, caret.y + caret.height * (event.code == key::up ? -0.5f : 1.5f)}),
                event.mods.shift
            );
        }

        return m_multiline;
    }

    case key::home:
    case key::end:
    {
        auto next = event.code == key::home ? std::size_t{0} : m_value.size();

        if(m_layout && !event.mods.control && !event.mods.system)
        {
            const auto caret = caret_bounds(*m_layout, m_caret);
            next = text_hit(
                *m_layout,
                {event.code == key::home ? -1.0f : m_layout->bounds.width + 1, caret.y + caret.height / 2}
            );
        }

        move_caret(next, event.mods.shift);

        return true;
    }

    case key::backspace:
    case key::delete_:
    {
        if(begin == end)
        {
            m_anchor = adjacent(event.code == key::delete_);
        }

        replace_selection({});

        return true;
    }

    case key::enter:
    {
        if(m_multiline)
        {
            replace_selection("\n");
        }
        else if(!event.repeat)
        {
            m_on_submit();
        }

        return true;
    }

    default:
    {
        return false;
    }
    }
}

std::string input_widget::normalize(std::string value) const
{
    std::string result;

    for(std::size_t index = 0; index < value.size();)
    {
        const auto first = static_cast<unsigned char>(value[index]);
        const std::size_t count = first < 0x80                     ? 1
                                  : first >= 0xC2 && first <= 0xDF ? 2
                                  : first >= 0xE0 && first <= 0xEF ? 3
                                  : first >= 0xF0 && first <= 0xF4 ? 4
                                                                   : 0;

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

        if((count == 2 && scalar < 0x80) || (count == 3 && scalar < 0x800) || (count == 4 && scalar < 0x10000) ||
           scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
        {
            throw std::invalid_argument("Input requires valid UTF-8");
        }

        if(first == '\r' || first == '\n')
        {
            result += m_multiline ? '\n' : ' ';

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

std::size_t input_widget::boundary(std::size_t offset) const noexcept
{
    while(offset > 0 && offset < m_value.size() && (static_cast<unsigned char>(m_value[offset]) & 0xC0) == 0x80)
    {
        --offset;
    }

    return offset;
}

std::size_t input_widget::adjacent(bool forward) const
{
    auto result = forward ? m_value.size() : 0;

    if(m_layout)
    {
        for(const auto& caret: m_layout->carets)
        {
            if(forward && caret.offset > m_caret)
            {
                result = std::min(result, caret.offset);
            }
            else if(!forward && caret.offset < m_caret)
            {
                result = std::max(result, caret.offset);
            }
        }
    }

    return result;
}

std::size_t input_widget::hit(float x, float y) const
{
    return m_layout ? text_hit(*m_layout, {x - content_bounds().x + m_scroll.x, y - content_bounds().y + m_scroll.y})
                    : 0;
}

void input_widget::move_caret(std::size_t next, bool extend)
{
    set_selection(extend ? m_anchor : next, next);
}

std::size_t input_widget::display_caret() const noexcept
{
    return m_preedit.empty() ? m_caret : std::min(m_anchor, m_caret) + m_preedit_caret;
}

void input_widget::prepare_text(text_service& service, float width) const
{
    const auto appearance = resolved_style();
    const auto identity = service.font_identity(appearance.font);

    if(m_layout && m_font_identity == identity && m_font_size == style().font_size && m_width == width &&
       m_layout->raster_scale == service.scale())
    {
        return;
    }

    auto text = m_value;

    if(!m_preedit.empty())
    {
        const auto [begin, end] = selection();
        text.replace(begin, end - begin, m_preedit);
    }

    m_layout = service.prepare(text, appearance.font, appearance.font_size, width);
    m_font_identity = identity;
    m_font_size = style().font_size;
    m_width = width;
}

void input_widget::changed_layout()
{
    m_layout.reset();
    m_reveal_caret = true;
    invalidate_layout();
}

void input_widget::notify_change()
{
    const auto value = m_value;
    m_on_change(value);
}

void input_widget::replace_selection(std::string replacement)
{
    const auto [begin, end] = selection();
    m_preedit.clear();

    if(begin == end && replacement.empty())
    {
        changed_layout();

        return;
    }

    auto candidate = m_value;
    candidate.replace(begin, end - begin, replacement);
    m_undo.push_back({m_value, m_anchor, m_caret});

    if(m_undo.size() > 128)
    {
        m_undo.erase(m_undo.begin());
    }

    m_redo.clear();
    m_value = std::move(candidate);
    m_anchor = m_caret = begin + replacement.size();
    changed_layout();
    notify_change();
}

void input_widget::restore(std::vector<snapshot>& source, std::vector<snapshot>& destination)
{
    if(source.empty())
    {
        return;
    }

    destination.push_back({m_value, m_anchor, m_caret});
    auto state = std::move(source.back());
    source.pop_back();
    m_value = std::move(state.value);
    m_anchor = state.anchor;
    m_caret = state.caret;
    changed_layout();
    notify_change();
}

} // namespace centui
